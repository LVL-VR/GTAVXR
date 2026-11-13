// build167 item 3 (bridge source-v16): the EXE-painted wrist buttons card (route GX_UI_BUTTONS, one fixed identity).
// Included inside Session AFTER hand_context_layer.inl. DEFAULT = beside the left-wrist map: gx_panel::buttonsBesideMap on the
// same left grip / wrist profile as wrist_layer (the EXE pokes that exact pose); any other anchor through place_panel(BUTTONS).
// Copy-only, like the hand context cards: its own swapchain, no shader, native state, simulation or readback.
UiPanel buttonsUiPanel;
const UiPixels*buttons_source()const{
    for(const auto&p:uiPixels)if(p.texture&&p.captured==frame&&gx_native_ui::buttonsItem(p.item))return &p;
    return nullptr;
}
bool buttons_layer(XrCompositionLayerQuad&q){
    if(overlayState.visible||panel||!space)return false;
    const UiPixels*source=buttons_source();if(!source)return false;
    const GxUiItem&r=source->item;if(r.width>uiMaxWidth||r.height>uiMaxHeight)return false;
    if(panelPlacements.panel[gx_panel::BUTTONS].anchor==gx_panel::DEFAULT&&(hudHandFrame!=frame||!(hudHandMask&1)))return false;
    const double s=gx_wrist::scale(wristProfiles.hand[0]);
    const GxPose p=gx_panel::buttonsBesideMap(gx_wrist::pose(hudGrip[0],wristProfiles.hand[0],0),s);
    q.space=space;q.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
    q.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT|XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
    q.pose.orientation={float(p.qx),float(p.qy),float(p.qz),float(p.qw)};q.pose.position={float(p.x),float(p.y),float(p.z)};
    const float wide=float(gx_panel::BUTTONS_WIDTH*s);q.size={wide,wide*float(r.height)/float(r.width)};
    if(!place_panel(q,gx_panel::BUTTONS))return false;
    auto&target=buttonsUiPanel;
    if(target.width!=r.width||target.height!=r.height){close_ui_eye(target.eye);target.width=r.width;target.height=r.height;}
    if(!target.eye.swap)create_ui_eye(target.eye,r.width,r.height);
    auto*texture=acquire_ui_eye(target.eye,r.width,r.height);
    demand(texture!=source->texture.Get(),"UI_BUTTONS_ALIAS");
    perf147.gpu(context.Get(),"buttons_copy",-1,0,[&](){swap_copy(texture,source->texture.Get());});release_ui_eye(target.eye);
    q.subImage.swapchain=target.eye.swap;q.subImage.imageArrayIndex=0;q.subImage.imageRect.offset={0,0};
    q.subImage.imageRect.extent={int(r.width),int(r.height)};
    if(frame<=5||frame%600==0)log("BUTTONS_UI frame="+std::to_string(frame)+" anchor="+
        std::to_string(panelPlacements.panel[gx_panel::BUTTONS].anchor)+" height="+std::to_string(r.height));
    return true;
}
void close_buttons_layer(){close_ui_eye(buttonsUiPanel.eye);buttonsUiPanel.width=buttonsUiPanel.height=0;}
