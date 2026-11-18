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
    if(!publish_ui_panel(q.subImage,buttonsUiPanel,*source,512,128,"buttons_copy"))return false;
    if(frame<=5||frame%600==0)log("BUTTONS_UI frame="+std::to_string(frame)+" anchor="+
        std::to_string(panelPlacements.panel[gx_panel::BUTTONS].anchor)+" height="+std::to_string(r.height));
    return true;
}
void close_buttons_layer(){close_ui_panel(buttonsUiPanel);}
