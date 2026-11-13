// Inside native_xr::Session. Called under the existing native context mutex.
menyoo_vr::Appearance menuAppearance=menyoo_vr::defaultAppearance();
void set_menu_appearance(const menyoo_vr::Appearance&s){
 demand(menyoo_vr::validAppearance(s),"MENU_APPEARANCE_ABI");
 if(memcmp(&s,&menuAppearance,sizeof(s))){menuAppearance=s;overlayUploaded=false;}
}
GxOverlay overlayState{};Eye overlayEye;uint32_t overlayRevision=0;bool overlayUploaded=false;
void set_overlay(const GxOverlay&s){demand(gx_ui::valid(s),"OVERLAY_ABI");overlayState=s;}
void upload_overlay(){
    if(!overlayState.visible)return;
    if(!overlayEye.swap){
        XrSwapchainCreateInfo sw{XR_TYPE_SWAPCHAIN_CREATE_INFO};sw.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        sw.format=selected_format;sw.sampleCount=1;sw.width=gx_ui::Width;sw.height=gx_ui::Height;sw.faceCount=sw.arraySize=sw.mipCount=1;
        check(perf147.cpu("xrCreateSwapchain",[&](){return api.CreateSwapchain(session,&sw,&overlayEye.swap);}),"CreateOverlay");uint32_t n=0;
        check(perf147.cpu("xrEnumerateSwapchainImages",[&](){return api.EnumerateSwapchainImages(overlayEye.swap,0,&n,nullptr);}),"OverlayImageCount");demand(n&&n<=16,"OVERLAY_IMAGE_COUNT");
        overlayEye.images.assign(n,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
        check(api.EnumerateSwapchainImages(overlayEye.swap,n,&n,reinterpret_cast<XrSwapchainImageBaseHeader*>(overlayEye.images.data())),"OverlayImages");
    }
    if(overlayUploaded&&overlayRevision==overlayState.revision)return;
    std::vector<unsigned char> pixels;
    if(overlayState.version==2){gx_ui::Bitmap painter;pixels=painter.paint(overlayState);}
    else{gx_menyoo_ui::Bitmap painter;pixels=painter.paint(overlayState,menuAppearance);}
    static_assert(int(gx_ui::Width)==int(gx_menyoo_ui::Width)&&int(gx_ui::Height)==int(gx_menyoo_ui::Height),"Menu painter extent");
    uint32_t index=0;XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    check(perf147.cpu("xrAcquireSwapchainImage",[&](){return api.AcquireSwapchainImage(overlayEye.swap,&acquire,&index);}),"OverlayAcquire");overlayEye.acquired=true;
    XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=1000000000;
    demand(perf147.cpu("xrWaitSwapchainImage",[&](){return api.WaitSwapchainImage(overlayEye.swap,&wait);})==XR_SUCCESS,"OVERLAY_WAIT");overlayEye.waited=true;
    demand(index<overlayEye.images.size()&&overlayEye.images[index].texture,"OVERLAY_INDEX");
    auto*t=overlayEye.images[index].texture;D3D11_TEXTURE2D_DESC d{};t->GetDesc(&d);
    demand(d.Width==gx_ui::Width&&d.Height==gx_ui::Height&&d.ArraySize==1&&d.MipLevels==1&&d.SampleDesc.Count==1&&
        swap_format(d.Format),"OVERLAY_TEXTURE");
    ComPtr<ID3D11Device> owner;t->GetDevice(&owner);demand(owner.Get()==device.Get(),"OVERLAY_DEVICE");
    if(swapRgba)for(size_t i=0;i+3<pixels.size();i+=4)std::swap(pixels[i],pixels[i+2]); // 167-neural A2: BGRA bytes -> RGBA image
    perf147.gpu(context.Get(),"overlay_upload",-1,0,[&](){context->UpdateSubresource(t,0,nullptr,pixels.data(),gx_ui::Width*4,0);});context->Flush();health("overlay_upload");
    XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};check(perf147.cpu("xrReleaseSwapchainImage",[&](){return api.ReleaseSwapchainImage(overlayEye.swap,&release);}),"OverlayRelease");
    overlayEye.acquired=overlayEye.waited=false;overlayUploaded=true;overlayRevision=overlayState.revision;
}
void close_overlay(){
    if(overlayEye.acquired){if(!overlayEye.waited){XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=1000000000;
        demand(perf147.cpu("xrWaitSwapchainImage",[&](){return api.WaitSwapchainImage(overlayEye.swap,&wait);})==XR_SUCCESS,"OVERLAY_CLEANUP_WAIT");overlayEye.waited=true;}
        XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};check(perf147.cpu("xrReleaseSwapchainImage",[&](){return api.ReleaseSwapchainImage(overlayEye.swap,&release);}),"OverlayCleanupRelease");overlayEye.acquired=overlayEye.waited=false;}
    if(overlayEye.swap){overlayEye.images.clear();check(api.DestroySwapchain(overlayEye.swap),"DestroyOverlay");overlayEye.swap={};}
    overlayUploaded=false;
}
