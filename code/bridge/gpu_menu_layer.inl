// Inside Session AFTER native_ui_layer. Reuses session-owned panel storage. Never sends menu pixels to a wrist or copies whole HUD.
UiPanel nativeMenuPanel;
bool native_menu_layer(XrCompositionLayerQuad&q){
 if(panel||!overlayState.visible||overlayState.version!=3||!head_space)return false;
 UiPixels*p=nullptr;for(auto&item:uiPixels)if(item.texture&&item.captured==frame&&gx_menyoo_route::belongs(item.item)){p=&item;break;}
 if(!p)return false;const auto&item=p->item;float physicalWidth=0,physicalHeight=0;
 if(!gx_menyoo_route::physicalSize(item.width,item.height,physicalWidth,physicalHeight)||
    item.width>uiMaxWidth||item.height>uiMaxHeight)return false;
 if(!publish_ui_panel(q.subImage,nativeMenuPanel,*p,1024,512,"menu_copy"))return false;
 q.space=head_space;q.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
 q.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT|XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
 q.pose.orientation.w=1;q.pose.position={0,0,-1.4f};q.size={physicalWidth,physicalHeight};return true;
}
void close_native_menu_layer(){close_ui_panel(nativeMenuPanel);}
