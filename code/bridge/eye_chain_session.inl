 
// and of the per-eye NR/SR chain (eye_chain_gpu.h). Included inside Session. Chain errors that are not handled by the
// chain itself (it falls back to the blit on its own) become runtime errors, i.e. the usual bridge fault.
    // swapchain images this session may write: BGRA always, RGBA only when the runtime offered no BGRA format
    bool swap_format(DXGI_FORMAT f)const{
        return f==DXGI_FORMAT_B8G8R8A8_UNORM||f==DXGI_FORMAT_B8G8R8A8_TYPELESS||f==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB||
            (swapRgba&&(f==DXGI_FORMAT_R8G8B8A8_UNORM||f==DXGI_FORMAT_R8G8B8A8_TYPELESS||f==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB));
    }
    // CopyResource (region=false) / CopySubresourceRegion (region=true) into a swapchain image; BGRA runtimes make exactly
    // the source-v16 call, RGBA runtimes draw the same bytes (gx_chain::Blitter, P4: byte-exact into RGBA/sRGB/typeless)
    void swap_copy(ID3D11Texture2D*target,ID3D11Texture2D*source,UINT x=0,UINT y=0,const D3D11_BOX*box=nullptr,bool region=false){
        if(!swapRgba){if(region)context->CopySubresourceRegion(target,0,x,y,0,source,0,box);else context->CopyResource(target,source);return;}
        try{swapBlit.init(device.Get());swapBlit.copy(context.Get(),target,x,y,source,box);}
        catch(const gx_chain::Error&e){throw std::runtime_error(std::string("XR_RGBA_COPY_")+e.stage);}
    }
    // after initialize: the EXE's render/output sizes and the Suite preset; 0 = NR/SR ready, 1 = bilinear blit
    int configure_eye_chain(const GxEyeChainConfig&k){
        demand(instance&&session&&!eyeChain.active()&&k.output_width==width&&k.output_height==height,"EYE_CHAIN_CONFIG");
        eyeChain.log=[this](const std::string&s){log(s);};eyeChain.timing=!compact_logging;
        try{return eyeChain.configure(device.Get(),context.Get(),k,chainDataDir,chainBridgeDir);}
        catch(const gx_chain::Error&e){throw std::runtime_error(std::string("EYE_CHAIN_")+e.stage);}
    }
    bool eye_chain_guides(const GxEyeGuides*g,ID3D11Resource*depth){
        if(!eyeChain.active())return false;
        try{eyeChain.guides(g,depth);}catch(const gx_chain::Error&e){throw std::runtime_error(std::string("EYE_CHAIN_")+e.stage);}
        return true;
    }
    bool set_neural_params(uint32_t enabled,uint32_t intensity,uint32_t compare){
        if(!eyeChain.active())return false;
        try{eyeChain.set_params(enabled,intensity,compare);}catch(const gx_chain::Error&e){throw std::runtime_error(std::string("EYE_CHAIN_")+e.stage);}
        return true;
    }
    void present_eye(unsigned e,uint64_t gen,ID3D11Texture2D*source,ID3D11Texture2D*target){
        try{eyeChain.present(context.Get(),e,gen,source,target);}catch(const gx_chain::Error&x){throw std::runtime_error(std::string("EYE_CHAIN_")+x.stage);}
    }
    void close_eye_chain(){eyeChain.close();swapBlit.close();}
