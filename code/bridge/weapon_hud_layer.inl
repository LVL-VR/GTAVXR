// Original native HUD, captured once after composition into transparent scratch.
// Reuses the wrist capture envelope; never keeps a reference to the native resource.
Eye weaponHudEye;uint64_t weaponHudFrame=0;unsigned weaponHudWidth=0,weaponHudHeight=0;
uint64_t weaponHudCaptures=0;
unsigned hudHandMask=0;uint64_t hudHandFrame=0;GxPose hudGrip[2]{};
void capture_weapon_hud(const GxWristCapture&r,ID3D11Texture2D*source){
    demand(!fault&&begun&&visible&&!panel&&source&&r.size==sizeof(r)&&r.version==1&&
        r.frame==frame&&r.generation==generation&&weaponHudFrame!=frame,"WEAPON_HUD_FRAME");
    D3D11_TEXTURE2D_DESC sd{};source->GetDesc(&sd);
    demand(r.x==0&&r.y==0&&r.width&&r.height&&r.width<=3840&&r.height<=2160&&
        sd.Width==r.width&&sd.Height==r.height&&sd.ArraySize==1&&sd.MipLevels==1&&sd.SampleDesc.Count==1&&
        (sd.Format==DXGI_FORMAT_B8G8R8A8_UNORM||sd.Format==DXGI_FORMAT_B8G8R8A8_TYPELESS||sd.Format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB),"WEAPON_HUD_SOURCE");
    ComPtr<ID3D11Device>owner;source->GetDevice(&owner);demand(owner.Get()==device.Get(),"WEAPON_HUD_DEVICE");
    demand(!weaponHudEye.swap||(weaponHudWidth==r.width&&weaponHudHeight==r.height),"WEAPON_HUD_RESIZE");
    if(!weaponHudEye.swap){
        XrSwapchainCreateInfo sw{XR_TYPE_SWAPCHAIN_CREATE_INFO};sw.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        sw.format=selected_format;sw.sampleCount=1;sw.width=r.width;sw.height=r.height;sw.faceCount=sw.arraySize=sw.mipCount=1;
        check(perf147.cpu("xrCreateSwapchain",[&](){return api.CreateSwapchain(session,&sw,&weaponHudEye.swap);}),"CreateWeaponHud");weaponHudWidth=r.width;weaponHudHeight=r.height;uint32_t n=0;
        check(perf147.cpu("xrEnumerateSwapchainImages",[&](){return api.EnumerateSwapchainImages(weaponHudEye.swap,0,&n,nullptr);}),"WeaponHudImageCount");demand(n&&n<=16,"WEAPON_HUD_IMAGE_COUNT");
        weaponHudEye.images.assign(n,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
        check(api.EnumerateSwapchainImages(weaponHudEye.swap,n,&n,reinterpret_cast<XrSwapchainImageBaseHeader*>(weaponHudEye.images.data())),"WeaponHudImages");
    }
    uint32_t index=0;XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    check(perf147.cpu("xrAcquireSwapchainImage",[&](){return api.AcquireSwapchainImage(weaponHudEye.swap,&acquire,&index);}),"WeaponHudAcquire");weaponHudEye.acquired=true;
    XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=1000000000;
    demand(perf147.cpu("xrWaitSwapchainImage",[&](){return api.WaitSwapchainImage(weaponHudEye.swap,&wait);})==XR_SUCCESS,"WEAPON_HUD_WAIT");weaponHudEye.waited=true;
    demand(index<weaponHudEye.images.size()&&weaponHudEye.images[index].texture,"WEAPON_HUD_INDEX");auto*target=weaponHudEye.images[index].texture;
    D3D11_TEXTURE2D_DESC td{};target->GetDesc(&td);target->GetDevice(owner.ReleaseAndGetAddressOf());
    demand(target!=source&&owner.Get()==device.Get()&&td.Width==r.width&&td.Height==r.height&&td.ArraySize==1&&td.MipLevels==1&&td.SampleDesc.Count==1&&
        swap_format(td.Format),"WEAPON_HUD_TARGET");
    perf147.gpu(context.Get(),"weapon_hud_copy",-1,0,[&](){swap_copy(target,source);});context->Flush();health("weapon_hud_copy");
    XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};check(perf147.cpu("xrReleaseSwapchainImage",[&](){return api.ReleaseSwapchainImage(weaponHudEye.swap,&release);}),"WeaponHudRelease");
    weaponHudEye.acquired=weaponHudEye.waited=false;weaponHudFrame=frame;
    ++weaponHudCaptures;
    if(weaponHudCaptures<=5||weaponHudCaptures%120==0)log("WEAPON_HUD frame="+std::to_string(frame)+" generation="+std::to_string(generation)+" size="+std::to_string(r.width)+","+std::to_string(r.height));
}
bool weapon_hud_layer(XrCompositionLayerQuad&q){
    if(weaponHudFrame!=frame||!weaponHudEye.swap||weaponHudEye.acquired||!head_space||overlayState.visible)return false;
    q.space=head_space;q.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
    q.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT|XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
    q.subImage.swapchain=weaponHudEye.swap;q.subImage.imageRect.extent={int32_t(weaponHudWidth),int32_t(weaponHudHeight)};
    q.pose.orientation.w=1;q.pose.position={0,0,-1.4f};q.size={2.2f,2.2f*float(weaponHudHeight)/float(weaponHudWidth)};return true;
}
void close_weapon_hud(){
    if(weaponHudEye.acquired){if(!weaponHudEye.waited){XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=1000000000;
        demand(perf147.cpu("xrWaitSwapchainImage",[&](){return api.WaitSwapchainImage(weaponHudEye.swap,&wait);})==XR_SUCCESS,"WEAPON_HUD_CLEANUP_WAIT");weaponHudEye.waited=true;}
        XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};check(perf147.cpu("xrReleaseSwapchainImage",[&](){return api.ReleaseSwapchainImage(weaponHudEye.swap,&release);}),"WeaponHudCleanupRelease");weaponHudEye.acquired=weaponHudEye.waited=false;}
    if(weaponHudEye.swap){weaponHudEye.images.clear();check(api.DestroySwapchain(weaponHudEye.swap),"DestroyWeaponHud");weaponHudEye.swap={};}
    weaponHudFrame=0;weaponHudWidth=weaponHudHeight=0;hudHandFrame=0;hudHandMask=0;
}
