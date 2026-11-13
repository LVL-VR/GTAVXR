// Actual Session capture/storage/cleanup; included by CPU service doubles too.
    // 167-neural B2: with the chain the world eye arrives at the render size and leaves at the swapchain (output) size
    bool compatible(const D3D11_TEXTURE2D_DESC&d)const {return eyeChain.active()?shaped(d,eyeChain.render_width(),eyeChain.render_height()):shaped(d,width,height);}
    bool shaped(const D3D11_TEXTURE2D_DESC&d,uint32_t w,uint32_t h)const {
        return d.Width==w&&d.Height==h&&d.ArraySize==1&&d.MipLevels==1&&d.SampleDesc.Count==1&&d.SampleDesc.Quality==0&&
            swap_format(d.Format); // 167-neural A2: BGRA, or RGBA when the runtime has no BGRA swapchains
    }
    void destroy_panel(){panelEye.images.clear();if(panelEye.swap){check(api.DestroySwapchain(panelEye.swap),"DestroyPanelSwapchain");panelEye.swap={};}panelWidth=panelHeight=0;}
    void ensure_panel(uint32_t w,uint32_t h){
        if(panelEye.swap&&panelWidth==w&&panelHeight==h)return;
        demand(!panelEye.acquired&&w&&h&&w<=4096&&h<=4096&&w<=uiMaxWidth&&h<=uiMaxHeight&&selected_format,"XR_PANEL_SIZE");
        destroy_panel();
        XrSwapchainCreateInfo sw{XR_TYPE_SWAPCHAIN_CREATE_INFO};sw.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        sw.format=selected_format;sw.sampleCount=1;sw.width=w;sw.height=h;sw.faceCount=sw.arraySize=sw.mipCount=1;
        check(perf147.cpu("xrCreateSwapchain",[&](){return api.CreateSwapchain(session,&sw,&panelEye.swap);}),"CreatePanelSwapchain");
        uint32_t n=0;check(perf147.cpu("xrEnumerateSwapchainImages",[&](){return api.EnumerateSwapchainImages(panelEye.swap,0,&n,nullptr);}),"PanelImageCount");demand(n&&n<=16,"XR_PANEL_IMAGE_COUNT");
        panelEye.images.assign(n,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
        check(api.EnumerateSwapchainImages(panelEye.swap,n,&n,reinterpret_cast<XrSwapchainImageBaseHeader*>(panelEye.images.data())),"PanelImages");
        panelWidth=w;panelHeight=h;log("PANEL_SWAPCHAIN width="+std::to_string(w)+" height="+std::to_string(h));
    }
    void capture(uint64_t id,uint64_t gen,unsigned e,ID3D11Texture2D*source) {
        demand(!fault&&begun&&visible&&id==frame&&gen==generation&&e<2&&copied==e&&source,"XR_CAPTURE_IDENTITY_OR_ORDER");
        // Flat pause/scope frames copy once into independent window-sized swapchains.
        // World eyes retain their exact runtime-driven shape and two copies.
        const bool flat=panel||scopePanel.active;
        D3D11_TEXTURE2D_DESC s{};source->GetDesc(&s);demand(flat?shaped(s,s.Width,s.Height):compatible(s),"XR_SOURCE_SHAPE");
        ComPtr<ID3D11Device> source_device;source->GetDevice(&source_device);demand(source_device.Get()==device.Get(),"XR_SOURCE_DEVICE");
        if(flat&&e==1){++copied;return;}
        if(panel)ensure_panel(s.Width,s.Height);
        else if(scopePanel.active)ensure_scope_panel(s.Width,s.Height);
        Eye&eye=scopePanel.active?scopePanelEye:panel?panelEye:eyes[e];
        const uint32_t targetWidth=scopePanel.active?scopePanelWidth:panel?panelWidth:width,
            targetHeight=scopePanel.active?scopePanelHeight:panel?panelHeight:height;uint32_t index=0;XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        check(perf147.cpu("xrAcquireSwapchainImage",[&](){return api.AcquireSwapchainImage(eye.swap,&ai,&index);}),"AcquireImage");eye.acquired=true;
        XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wi.timeout=1000000000;
        demand(perf147.cpu("xrWaitSwapchainImage",[&](){return api.WaitSwapchainImage(eye.swap,&wi);})==XR_SUCCESS,"XR_WAIT_NOT_READY");eye.waited=true;
        try {
            demand(index<eye.images.size(),"XR_IMAGE_INDEX");auto*target=eye.images[index].texture;demand(target,"XR_NULL_IMAGE");
            D3D11_TEXTURE2D_DESC d{};target->GetDesc(&d);demand(shaped(d,targetWidth,targetHeight)&&source!=target,"XR_TARGET_SHAPE_OR_ALIAS");
            ComPtr<ID3D11Device> target_device;target->GetDevice(&target_device);demand(target_device.Get()==device.Get(),"XR_TARGET_DEVICE");
            perf147.gpu(context.Get(),flat?"flat_copy":"eye_copy",int(e),flat?0:-1,[&](){if(!flat&&eyeChain.active())present_eye(e,gen,source,target);else swap_copy(target,source);});context->Flush();health("capture_after");
            // First pair + four spaced pairs only; never continuous GPU readback.
            // These frame IDs also have the existing pose telemetry (multiples of120).
            if(!flat&&first_image&&(submitted==0||frame==120||frame==1200||frame==2400||frame==3600))first_image(source,e);
        } catch(...) {
            XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            if(XR_SUCCEEDED(perf147.cpu("xrReleaseSwapchainImage",[&](){return api.ReleaseSwapchainImage(eye.swap,&ri);})))eye.acquired=eye.waited=false;
            throw;
        }
        XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};check(perf147.cpu("xrReleaseSwapchainImage",[&](){return api.ReleaseSwapchainImage(eye.swap,&ri);}),"ReleaseImage");
        eye.acquired=eye.waited=false;++copied;
    }
    void release_frame_images(){
        // A timed-out acquired image must finish waiting before release. No copy on cleanup.
        for(Eye*cleanupEye:{&eyes[0],&eyes[1],&panelEye,&scopePanelEye})if(cleanupEye->acquired){Eye&e=*cleanupEye;if(!e.waited){XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wi.timeout=1000000000;demand(perf147.cpu("xrWaitSwapchainImage",[&](){return api.WaitSwapchainImage(e.swap,&wi);})==XR_SUCCESS,"XR_CLEANUP_IMAGE_TIMEOUT");e.waited=true;}XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};check(perf147.cpu("xrReleaseSwapchainImage",[&](){return api.ReleaseSwapchainImage(e.swap,&ri);}),"CleanupRelease");e.acquired=e.waited=false;}
    }
