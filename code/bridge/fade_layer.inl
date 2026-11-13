// build167 item 4 (bridge source-v16): the head inside a wall. Included inside Session after buttons_layer.inl. A black
// head-locked quad right above the world projection (under every HUD / menu layer), its alpha from the EXE
// (GtavxrSetViewFade1, sent every frame while dark); not refreshed for 90 frames = gone. Copy-only like the UI cards.
Eye fadeEye;ComPtr<ID3D11Texture2D> fadeSource;float fadeAlpha=0;uint64_t fadeFrame=0;unsigned fadeByte=256;
void set_view_fade(float a){demand(a==a&&a>=0.f&&a<=1.f,"VIEW_FADE_RANGE");fadeAlpha=a;fadeFrame=frame;}
bool fade_layer(XrCompositionLayerQuad&q){
    if(!head_space||!(fadeAlpha>.004f)||frame>fadeFrame+90)return false;
    const unsigned w=8,h=8,alpha=unsigned(fadeAlpha*255.f+.5f);
    if(!fadeSource){D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
        d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;demand(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&fadeSource)),"FADE_SOURCE");fadeByte=256;}
    if(alpha!=fadeByte){uint32_t pixels[w*h];for(auto&p:pixels)p=uint32_t(alpha)<<24; // BGRA: black, straight alpha
        context->UpdateSubresource(fadeSource.Get(),0,nullptr,pixels,w*4,0);fadeByte=alpha;}
    if(!fadeEye.swap)create_ui_eye(fadeEye,w,h);
    auto*texture=acquire_ui_eye(fadeEye,w,h);
    perf147.gpu(context.Get(),"fade_copy",-1,0,[&](){swap_copy(texture,fadeSource.Get());});release_ui_eye(fadeEye);
    q.space=head_space;q.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
    q.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT|XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
    q.subImage.swapchain=fadeEye.swap;q.subImage.imageArrayIndex=0;q.subImage.imageRect.offset={0,0};q.subImage.imageRect.extent={int(w),int(h)};
    q.pose.orientation={0,0,0,1};q.pose.position={0,0,-.2f};q.size={1.2f,1.2f}; // ~143 degrees at 20 cm: past the Quest field of view
    if(frame<=5||frame%600==0)log("VIEW_FADE frame="+std::to_string(frame)+" alpha="+std::to_string(alpha));
    return true;
}
void close_fade_layer(){close_ui_eye(fadeEye);fadeSource.Reset();fadeByte=256;fadeAlpha=0;}
