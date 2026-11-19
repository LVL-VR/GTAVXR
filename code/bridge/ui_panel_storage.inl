// Included in Session. Composite a logical crop from session-owned storage.
// A color swapchain destroy during gameplay invalidates OFXR's stereo history.
struct UiPanel {
    Eye eye;
    uint32_t width=0,height=0;
    uint64_t retryFrame=0;
    std::vector<Eye> retained;
    std::vector<ComPtr<ID3D11RenderTargetView>> clearViews;
};
std::array<UiPanel,8> uiPanels;
UiPanel wantedUiPanel;
UiPanel subtitleUiPanel;

static uint32_t ui_panel_capacity(uint32_t requested,uint32_t floor,uint32_t limit){
    uint32_t capacity=(std::min)(floor,limit);
    while(capacity<requested)
        capacity+=(std::min)(capacity,limit-capacity);
    return capacity;
}

void close_ui_panel(UiPanel& storage){
    storage.clearViews.clear();
    close_ui_eye(storage.eye);
    for(auto& eye:storage.retained)close_ui_eye(eye);
    storage.retained.clear();
    storage.width=storage.height=0;
    storage.retryFrame=0;
}

static bool ui_capacity_refused(XrResult result){
    return result==XR_ERROR_LIMIT_REACHED||result==XR_ERROR_OUT_OF_MEMORY;
}

void grow_ui_panel(UiPanel& storage,uint32_t w,uint32_t h,uint32_t floorW,uint32_t floorH,
                   uint32_t maxW,uint32_t maxH){
    if((storage.width>=w&&storage.height>=h)||frame<storage.retryFrame)return;
    demand(!storage.eye.acquired,"UI_PANEL_ACQUIRED");
    // Keep the old image live until we know that the runtime accepted the
    // replacement. Any partial allocation remains owned for session cleanup.
    storage.retained.emplace_back();
    Eye& replacement=storage.retained.back();
    const uint32_t exactW=(std::max)(w,storage.width),exactH=(std::max)(h,storage.height);
    uint32_t capacityW=(std::max)(storage.width,ui_panel_capacity(w,floorW,maxW));
    uint32_t capacityH=(std::max)(storage.height,ui_panel_capacity(h,floorH,maxH));
    XrResult result=try_create_ui_eye(replacement,capacityW,capacityH);
    if(ui_capacity_refused(result)&&!storage.eye.swap&&!replacement.swap&&(capacityW!=exactW||capacityH!=exactH)){
        capacityW=exactW;capacityH=exactH;
        result=try_create_ui_eye(replacement,capacityW,capacityH);
    }
    if(ui_capacity_refused(result)&&!replacement.swap){
        storage.retained.pop_back();
        storage.retryFrame=frame+120;
        log("UI_PANEL_CAPACITY result="+std::to_string(int(result))+
            " fallback="+(storage.eye.swap?"resample":"skip_until_retry"));
        return;
    }
    check(result,"UiCreate");
    std::swap(storage.eye,replacement);
    if(!replacement.swap)storage.retained.pop_back();
    storage.clearViews.clear();
    storage.width=capacityW;storage.height=capacityH;
    storage.retryFrame=0;
}

bool publish_ui_panel(XrSwapchainSubImage& image,UiPanel& storage,const UiPixels& source,
                      uint32_t floorWidth,uint32_t floorHeight,const char* label){
    const auto& crop=source.item;
    const uint32_t maxWidth=(std::min)(uiMaxWidth,3840u);
    const uint32_t maxHeight=(std::min)(uiMaxHeight,2160u);
    demand(source.texture&&crop.width&&crop.width<=maxWidth&&crop.height&&crop.height<=maxHeight,
           "UI_PANEL_EXTENT");
    grow_ui_panel(storage,crop.width,crop.height,floorWidth,floorHeight,maxWidth,maxHeight);
    if(!storage.eye.swap)return false;
    uint32_t width=crop.width,height=crop.height;
    if(width>storage.width){
        height=(std::max)(1u,height*storage.width/width);
        width=storage.width;
    }
    if(height>storage.height){
        width=(std::max)(1u,width*storage.height/height);
        height=storage.height;
    }
    const bool scaled=width!=crop.width||height!=crop.height;
    const bool padded=width!=storage.width||height!=storage.height;
    ComPtr<ID3D11ShaderResourceView> readable;
    if(scaled){
        swapBlit.init(device.Get());
        readable=swapBlit.readable(context.Get(),source.texture.Get());
    }
    uint32_t index=0;
    auto* target=acquire_ui_eye(storage.eye,storage.width,storage.height,&index);
    demand(target!=source.texture.Get(),"UI_PANEL_ALIAS");

    perf147.gpu(context.Get(),label,-1,0,[&](){
        if(padded){
            // Reuse the views even as the runtime rotates its swapchain images.
            if(storage.clearViews.empty())storage.clearViews.resize(storage.eye.images.size());
            auto& clear=storage.clearViews.at(index);
            if(!clear){
                D3D11_RENDER_TARGET_VIEW_DESC desc{};
                desc.Format=static_cast<DXGI_FORMAT>(selected_format);
                desc.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
                demand(SUCCEEDED(device->CreateRenderTargetView(target,&desc,&clear)),"UI_PANEL_RTV");
            }
            const float zero[4]={0,0,0,0};
            context->ClearRenderTargetView(clear.Get(),zero);
        }
        if(scaled)
            swapBlit.draw(context.Get(),target,0,0,width,height,readable.Get(),
                          0,0,crop.width,crop.height,crop.width,crop.height,false);
        else
            swap_copy(target,source.texture.Get(),0,0,nullptr,padded);
    });
    release_ui_eye(storage.eye);
    image={storage.eye.swap,{{0,0},{int32_t(width),int32_t(height)}},0};
    return true;
}
