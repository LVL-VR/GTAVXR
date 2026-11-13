// Session members; also compiled unchanged by the CPU service-double control.
gx_scope_panel::Frame scopePanel;
Eye scopePanelEye;
uint32_t scopePanelWidth=0,scopePanelHeight=0;
uint64_t scope_frames=0;
void reset_frame_presentation(){panel=false;scopePanel.reset();}
bool set_scope_panel(const GxScopePanel* p){
    return !fault&&scopePanel.set(begun,copied,panel,p);
}
bool set_menu_panel(){
    if(scopePanel.active)return false; // optional mode conflict must not fault the session
    demand(begun&&!copied,"MENU_FRAME_ORDER"); // preserve existing menu ordering contract
    panel=true;return true;
}
void destroy_scope_panel(){
    demand(!scopePanelEye.acquired,"XR_SCOPE_PANEL_ACQUIRED");
    scopePanelEye.images.clear();
    if(scopePanelEye.swap){check(api.DestroySwapchain(scopePanelEye.swap),"DestroyScopePanelSwapchain");scopePanelEye.swap={};}
    scopePanelWidth=scopePanelHeight=0;scopePanel.reset();
}
void ensure_scope_panel(uint32_t w,uint32_t h){
    if(scopePanelEye.swap&&scopePanelWidth==w&&scopePanelHeight==h)return;
    demand(!scopePanelEye.acquired&&w&&h&&w<=4096&&h<=4096&&w<=uiMaxWidth&&h<=uiMaxHeight&&selected_format,"XR_SCOPE_PANEL_SIZE");
    // Recreating the storage must retain this frame's already validated request.
    const gx_scope_panel::Frame requested=scopePanel;
    destroy_scope_panel();scopePanel=requested;
    XrSwapchainCreateInfo sw{XR_TYPE_SWAPCHAIN_CREATE_INFO};sw.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
    sw.format=selected_format;sw.sampleCount=1;sw.width=w;sw.height=h;sw.faceCount=sw.arraySize=sw.mipCount=1;
    check(api.CreateSwapchain(session,&sw,&scopePanelEye.swap),"CreateScopePanelSwapchain");
    uint32_t n=0;check(api.EnumerateSwapchainImages(scopePanelEye.swap,0,&n,nullptr),"ScopePanelImageCount");demand(n&&n<=16,"XR_SCOPE_PANEL_IMAGE_COUNT");
    scopePanelEye.images.assign(n,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
    check(api.EnumerateSwapchainImages(scopePanelEye.swap,n,&n,reinterpret_cast<XrSwapchainImageBaseHeader*>(scopePanelEye.images.data())),"ScopePanelImages");
    scopePanelWidth=w;scopePanelHeight=h;log("SCOPE_PANEL_SWAPCHAIN width="+std::to_string(w)+" height="+std::to_string(h));
}
void scope_panel_layer(XrCompositionLayerQuad& q){
    demand(head_space!=XR_NULL_HANDLE,"XR_SCOPE_PANEL_SPACE");
    demand(scopePanelEye.swap&&scopePanelWidth&&scopePanelHeight,"XR_SCOPE_PANEL_IMAGE");
    gx_scope_panel::Size physical;
    demand(scopePanel.active&&gx_scope_panel::size(scopePanel.request,scopePanelWidth,scopePanelHeight,physical),"XR_SCOPE_PANEL_GEOMETRY");
    q={XR_TYPE_COMPOSITION_LAYER_QUAD};q.space=head_space;
    q.eyeVisibility=static_cast<XrEyeVisibility>(gx_scope_panel::visibility(scopePanel.request.eyes));
    q.subImage.swapchain=scopePanelEye.swap;q.subImage.imageArrayIndex=0;
    q.subImage.imageRect.offset={0,0};q.subImage.imageRect.extent={int32_t(scopePanelWidth),int32_t(scopePanelHeight)};
    q.pose.orientation.w=1;q.pose.position={0,0,-float(scopePanel.request.distance_m)};
    q.size={float(physical.width),float(physical.height)}; // opaque: layerFlags remains zero
}

