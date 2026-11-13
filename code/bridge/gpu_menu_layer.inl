// Inside Session AFTER native_ui_layer. Reuses owned UI storage/copy/resize/
// acquire-wait-release. Never sends menu pixels to a wrist or copies whole HUD.
UiPanel nativeMenuPanel;
bool native_menu_layer(XrCompositionLayerQuad&q){
 if(panel||!overlayState.visible||overlayState.version!=3||!head_space)return false;
 UiPixels*p=nullptr;for(auto&item:uiPixels)if(item.texture&&item.captured==frame&&gx_menyoo_route::belongs(item.item)){p=&item;break;}
 if(!p)return false;const auto&item=p->item;float physicalWidth=0,physicalHeight=0;
 if(!gx_menyoo_route::physicalSize(item.width,item.height,physicalWidth,physicalHeight)||
    item.width>uiMaxWidth||item.height>uiMaxHeight)return false;
 if(nativeMenuPanel.width!=item.width||nativeMenuPanel.height!=item.height){
  close_ui_eye(nativeMenuPanel.eye);nativeMenuPanel.width=item.width;nativeMenuPanel.height=item.height;
 }
 if(!nativeMenuPanel.eye.swap)create_ui_eye(nativeMenuPanel.eye,item.width,item.height);
 auto*target=acquire_ui_eye(nativeMenuPanel.eye,item.width,item.height);demand(target!=p->texture.Get(),"NATIVE_MENU_ALIAS");
 perf147.gpu(context.Get(),"menu_copy",-1,0,[&](){swap_copy(target,p->texture.Get());});release_ui_eye(nativeMenuPanel.eye);
 q.space=head_space;q.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
 q.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT|XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
 q.subImage.swapchain=nativeMenuPanel.eye.swap;q.subImage.imageRect.extent={static_cast<int32_t>(item.width),static_cast<int32_t>(item.height)};
 q.pose.orientation.w=1;q.pose.position={0,0,-1.4f};q.size={physicalWidth,physicalHeight};return true;
}
void close_native_menu_layer(){close_ui_eye(nativeMenuPanel.eye);nativeMenuPanel.width=nativeMenuPanel.height=0;}
