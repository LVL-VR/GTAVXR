// Included inside Session AFTER native_ui_layer. Uses only owned cropped pixels
// and the very same LOCAL hand/head poses already located for this XR frame.
std::array<UiPanel,2> handUiPanels;
bool hand_context_layer(XrCompositionLayerQuad&q,unsigned hand){
    if(hand>=2||overlayState.visible||panel||!space)return false;
    UiPixels*source=nullptr;
    for(auto&p:uiPixels)if(p.texture&&p.captured==frame&&gx_native_ui::handRoute(p.item.route)&&
        gx_native_ui::routeHand(p.item.route)==hand){
        if(source){if(frame<=5||frame%120==0)log("UI_GAP reason=hand_context_conflict hand="+std::to_string(hand));return false;}
        source=&p;
    }
    if(!source)return false;
    const auto head=pod(shared086::midpoint(pose(rendered[0].pose),pose(rendered[1].pose)));
    if(!gx_hand_display::fresh(hand,source->captured,hudHandFrame,frame,hudHandMask,hudGrip[hand],head))return false;
    const GxUiItem&r=source->item;
    // New route semantics require a qualified owner+hand identity. Old movie or
    // generic HUD IDs can never become a left/right local contextual card.
    if(!gx_native_ui::handItem(r)||r.width>uiMaxWidth||r.height>uiMaxHeight)return false;
    if(!publish_ui_panel(q.subImage,handUiPanels[hand],*source,512,128,"hand_copy"))return false;
    const GxPose p=gx_hand_display::nearHand(hand,hudGrip[hand],head);
    q.space=space;q.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
    q.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT|XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
    q.pose.orientation={float(p.qx),float(p.qy),float(p.qz),float(p.qw)};
    q.pose.position={float(p.x),float(p.y),float(p.z)};q.size={.24f,.24f*float(r.height)/r.width};
    if(frame<=5||frame%120==0)log("HAND_UI frame="+std::to_string(frame)+" generation="+std::to_string(generation)+
        " hand="+std::to_string(hand)+" id="+std::to_string(r.id)+" route="+std::to_string(r.route));
    return true;
}
void close_hand_contexts(){for(auto& p:handUiPanels)close_ui_panel(p);}

