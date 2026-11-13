 
Eye wristEye;uint64_t wristFrame=0,wristHandFrame=0;unsigned wristWidth=0,wristHeight=0;
GxPose wristGrip{};bool wristHandValid=false;
static constexpr unsigned WristExtent=1024;
void capture_wrist(const GxWristCapture&r,ID3D11Texture2D*source){
    demand(!fault&&begun&&visible&&!panel&&source&&r.size==sizeof(r)&&r.version==1&&
        r.frame==frame&&r.generation==generation&&wristFrame!=frame,"WRIST_FRAME");
    D3D11_TEXTURE2D_DESC sd{};source->GetDesc(&sd);
    demand(r.width&&r.height&&r.width<=WristExtent&&r.height<=WristExtent&&r.x<=sd.Width&&r.y<=sd.Height&&
        r.width<=sd.Width-r.x&&r.height<=sd.Height-r.y&&sd.ArraySize==1&&sd.MipLevels==1&&sd.SampleDesc.Count==1&&
        (sd.Format==DXGI_FORMAT_B8G8R8A8_UNORM||sd.Format==DXGI_FORMAT_B8G8R8A8_TYPELESS||sd.Format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB),"WRIST_SOURCE");
    ComPtr<ID3D11Device>owner;source->GetDevice(&owner);demand(owner.Get()==device.Get(),"WRIST_DEVICE");
    if(!wristEye.swap){
        XrSwapchainCreateInfo sw{XR_TYPE_SWAPCHAIN_CREATE_INFO};sw.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        sw.format=selected_format;sw.sampleCount=1;sw.width=sw.height=WristExtent;sw.faceCount=sw.arraySize=sw.mipCount=1;
        check(perf147.cpu("xrCreateSwapchain",[&](){return api.CreateSwapchain(session,&sw,&wristEye.swap);}),"CreateWrist");uint32_t n=0;
        check(perf147.cpu("xrEnumerateSwapchainImages",[&](){return api.EnumerateSwapchainImages(wristEye.swap,0,&n,nullptr);}),"WristImageCount");demand(n&&n<=16,"WRIST_IMAGE_COUNT");
        wristEye.images.assign(n,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
        check(api.EnumerateSwapchainImages(wristEye.swap,n,&n,reinterpret_cast<XrSwapchainImageBaseHeader*>(wristEye.images.data())),"WristImages");
    }
    uint32_t index=0;XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    check(perf147.cpu("xrAcquireSwapchainImage",[&](){return api.AcquireSwapchainImage(wristEye.swap,&acquire,&index);}),"WristAcquire");wristEye.acquired=true;
    XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=1000000000;
    demand(perf147.cpu("xrWaitSwapchainImage",[&](){return api.WaitSwapchainImage(wristEye.swap,&wait);})==XR_SUCCESS,"WRIST_WAIT");wristEye.waited=true;
    demand(index<wristEye.images.size()&&wristEye.images[index].texture,"WRIST_INDEX");auto*target=wristEye.images[index].texture;
    D3D11_TEXTURE2D_DESC td{};target->GetDesc(&td);target->GetDevice(owner.ReleaseAndGetAddressOf());
    demand(target!=source&&owner.Get()==device.Get()&&td.Width==WristExtent&&td.Height==WristExtent&&td.ArraySize==1&&td.MipLevels==1&&td.SampleDesc.Count==1&&
        swap_format(td.Format),"WRIST_TARGET");
    D3D11_BOX box{r.x,r.y,0,r.x+r.width,r.y+r.height,1};
    perf147.gpu(context.Get(),"wrist_copy",-1,0,[&](){swap_copy(target,source,0,0,&box,true);});context->Flush();health("wrist_copy");
    XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};check(perf147.cpu("xrReleaseSwapchainImage",[&](){return api.ReleaseSwapchainImage(wristEye.swap,&release);}),"WristRelease");
    wristEye.acquired=wristEye.waited=false;wristFrame=frame;wristWidth=r.width;wristHeight=r.height;
    if(frame<=5||frame%120==0)log("WRIST frame="+std::to_string(frame)+" generation="+std::to_string(generation)+" rect="+std::to_string(r.x)+","+std::to_string(r.y)+","+std::to_string(r.width)+","+std::to_string(r.height));
}
bool wrist_layer(XrCompositionLayerQuad&q,bool geometryOnly=false){
    if(!geometryOnly&&!gx_ui_visibility::enabled(uiVisibility,gx_ui_visibility::Map))return false;
    // build167 items 1/2: a map on the head / room / vehicle does not need the left hand
    if(wristFrame!=frame||(!panel_free(gx_panel::MAP)&&(wristHandFrame!=frame||!wristHandValid))||!wristEye.swap||wristEye.acquired)return false;
    // Back-of-left-wrist watch: quad front(+Z) maps to grip up(+Y), text up
    // maps to grip forward(-Z). Pose stays in the SAME LOCAL space/time as eyes.
    q.space=space;q.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
    q.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT|XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
    q.subImage.swapchain=wristEye.swap;q.subImage.imageRect.extent={int32_t(wristWidth),int32_t(wristHeight)};
    const GxPose adjusted=gx_wrist::pose(wristGrip,wristProfiles.hand[0],0);
    q.pose.orientation={float(adjusted.qx),float(adjusted.qy),float(adjusted.qz),float(adjusted.qw)};
    q.pose.position={float(adjusted.x),float(adjusted.y),float(adjusted.z)};
    const float wide=.28f*gx_wrist::scale(wristProfiles.hand[0]);
    q.size={wide,wide*float(wristHeight)/float(wristWidth)};return place_panel(q,gx_panel::MAP);
}
void close_wrist(){
    if(wristEye.acquired){if(!wristEye.waited){XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=1000000000;
        demand(perf147.cpu("xrWaitSwapchainImage",[&](){return api.WaitSwapchainImage(wristEye.swap,&wait);})==XR_SUCCESS,"WRIST_CLEANUP_WAIT");wristEye.waited=true;}
        XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};check(perf147.cpu("xrReleaseSwapchainImage",[&](){return api.ReleaseSwapchainImage(wristEye.swap,&release);}),"WristCleanupRelease");wristEye.acquired=wristEye.waited=false;}
    if(wristEye.swap){wristEye.images.clear();check(api.DestroySwapchain(wristEye.swap),"DestroyWrist");wristEye.swap={};}
    wristFrame=wristHandFrame=0;wristHandValid=false;
}
