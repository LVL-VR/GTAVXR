// Inside Session after native_ui_layer. Physical sniper lens, not a wrist or
// head-locked HUD. Uses SAME XR LOCAL space/time as the native scene eyes.
GxScopeOptic scopeOptics[2]{};gx_scope_crop::Renderer scopeCrop[2];Eye scopeEye[2][2];
gx_scope_reticle::Cache scopeReticles;GxScopeReticleBinding scopeReticleBindings[2]{};
uint64_t scopeLayerFrame[2]{};GxPose scopeLayerPose[2]{};unsigned scopeViewingMask=0;uint64_t scopeViewingFrame=0,scopeViewingGeneration=0,scopeViewingTick=0;
bool set_scope_optic(const GxScopeOptic&r){
 if(r.hand>=2)return false;
 // Release (flags 0) uses the v1 shape; the EXE always resets version/reserved when it drops a monocular packet.
 if(!r.flags&&r.size==sizeof(r)&&r.version==1&&!r.reserved&&r.frame==frame&&r.generation==generation){
  scopeOptics[r.hand]=GxScopeOptic();scopeLayerFrame[r.hand]=0;scopeViewingMask&=~(1u<<r.hand);return true;
 }
 if(fault||!begun||!visible||panel||scopePanel.active||!scopeSourceCaptureEnabled||!gx_scope_optic::valid(r,frame,generation))return false;
 scopeOptics[r.hand]=r;return true;
}
bool capture_scope_reticle(const GxScopeReticleCapture&r,ID3D11Texture2D*source){
 if(fault||!begun||!visible||panel||scopePanel.active||!scopeSourceCaptureEnabled||r.frame!=frame||r.generation!=generation)return false;
 const bool accepted=perf147.gpu(context.Get(),"scope_reticle",-1,-1,[&](){return scopeReticles.capture(context.Get(),source,r,static_cast<DXGI_FORMAT>(selected_format));});
 health("scope_reticle_copy");return accepted;
}
bool bind_scope_reticle(const GxScopeReticleBinding&r){
 if(r.size!=sizeof(r)||r.version!=1||r.hand>=2||r.frame!=frame||r.generation!=generation||fault||!begun||!visible||panel||scopePanel.active)return false;
 if(r.reticleHash&&!gx_scope_optic::valid(scopeOptics[r.hand],frame,generation))return false;
 scopeReticleBindings[r.hand]=r;scopeCrop[r.hand].invalidate();scopeLayerFrame[r.hand]=0;return true;
}
bool scope_local_lens(const GxScopeOptic&r,eye036::Matrix&local,gx_scope_pupil::Lens&lens,GxPose&quadPose){
 try {
 eye036::Matrix camera{},world{};std::copy_n(r.cameraReference,16,camera.begin());std::copy_n(r.worldLens,16,world.begin());
 const auto mapping=eye036::mul(eye036::inverse(reference084::xr_reference(pose(r.xrReference))),reference084::level(camera,{0,0,1}));
 local=eye036::mul(world,eye036::inverse(mapping));if(!gx_scope_optic::pose(local.data(),quadPose))return false;
 lens.center=gx_scope_eye::V(local[12],local[13],local[14]);
 lens.axes.right=gx_scope_eye::V(local[0],local[1],local[2]);lens.axes.up=gx_scope_eye::V(local[4],local[5],local[6]);
 lens.axes.forward=gx_scope_eye::V(-local[8],-local[9],-local[10]);lens.radius=r.radius;
 lens.minEyeRelief=r.minEyeRelief;lens.maxEyeRelief=r.maxEyeRelief;lens.geometryQualified=true;return lens.valid();
 } catch(const std::exception&) {return false;} // Optional optic math cannot turn off healthy VR.
}
// build141 E (user brief 2026-10-14): MONOCULAR sniper view. ONE head-locked circular quad, visible to the selected eye
// only, 1 m along that eye's rendered forward, covering GxScopeMonocularDisplayHalfAngle; the other eye keeps the normal
// projection. No pupil/eye-relief gate. Crop = the scope cone (lens axes, halfAngle) of that eye's FINAL image with
// magnification 1: the display size applies the zoom once, tan(0.40)/tan(halfAngle). Source contains the weapon (NOT clean).
// The dark periphery was only a discussed option (user), so no ring is drawn; the shader's disc alpha keeps the world
// visible around the image.
bool scope_monocular_layer(unsigned hand,const GxScopeOptic&r,const gx_scope_pupil::Lens&lens,XrCompositionLayerQuad*out,unsigned capacity,unsigned&count){
 const unsigned eye=gx_scope_optic::monocularEye(r);if(eye>=2||count>=capacity)return false;
 const auto eyeMatrix=pose075::pose(pose(rendered[eye].pose));
 gx_scope_eye::Axes axes;axes.right=gx_scope_eye::V(eyeMatrix[0],eyeMatrix[1],eyeMatrix[2]);
 axes.up=gx_scope_eye::V(eyeMatrix[4],eyeMatrix[5],eyeMatrix[6]);axes.forward=gx_scope_eye::V(-eyeMatrix[8],-eyeMatrix[9],-eyeMatrix[10]);
 gx_scope_eye::Fov fov;fov.left=rendered[eye].fov.angleLeft;fov.right=rendered[eye].fov.angleRight;
 fov.up=rendered[eye].fov.angleUp;fov.down=rendered[eye].fov.angleDown;gx_scope_eye::Mapping image;float mapping[12]{};
 if(!gx_scope_eye::build(axes,fov,lens.axes,r.halfAngle,1.0,image)||!gx_scope_eye::constants(image,mapping))return false;
 if(!scopeCrop[hand].current(true,eye,frame,generation)){
  if(!scopeCropAttempted[hand]){scopeCropAttempted[hand]=true;scopeCropReady[hand]=scopeCrop[hand].initialize(device.Get(),ScopeCropVs,sizeof(ScopeCropVs),ScopeReticlePs,sizeof(ScopeReticlePs),static_cast<DXGI_FORMAT>(selected_format));}
  if(!scopeCropReady[hand])return false;
  const GxScopeReticleBinding&binding=scopeReticleBindings[hand];unsigned alpha=0;ID3D11ShaderResourceView*reticle=nullptr;
  if(binding.frame==frame&&binding.generation==generation)reticle=scopeReticles.view(binding.reticleHash,frame,generation,alpha);
  if(!perf147.gpu(context.Get(),"scope_draw",int(eye),int(hand),[&](){return scopeCrop[hand].draw(context.Get(),scopeSources,eye,frame,generation,mapping,reticle,alpha,true);})){health("scope_monocular_draw");return false;}
  scopeLayerFrame[hand]=frame;
 }
 ID3D11Texture2D*source=scopeCrop[hand].image(eye,frame,generation);if(!source||scopeLayerFrame[hand]!=frame)return false;
 if(!scopeEye[hand][eye].swap)create_ui_eye(scopeEye[hand][eye],512,512);
 ID3D11Texture2D*target=acquire_ui_eye(scopeEye[hand][eye],512,512);
 perf147.gpu(context.Get(),"scope_copy",int(eye),int(hand),[&](){context->CopyResource(target,source);});release_ui_eye(scopeEye[hand][eye]);
 const double size=2*tan(GxScopeMonocularDisplayHalfAngle); // at 1 m
 XrCompositionLayerQuad&q=out[count++];q={XR_TYPE_COMPOSITION_LAYER_QUAD};q.space=space;
 q.eyeVisibility=eye?XR_EYE_VISIBILITY_RIGHT:XR_EYE_VISIBILITY_LEFT;
 q.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT|XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
 q.subImage.swapchain=scopeEye[hand][eye].swap;q.subImage.imageRect.extent={512,512};
 q.pose.orientation=rendered[eye].pose.orientation;
 q.pose.position={float(eyeMatrix[12]+axes.forward.x),float(eyeMatrix[13]+axes.forward.y),float(eyeMatrix[14]+axes.forward.z)};
 q.size={float(size),float(size)};return true;
}
bool scope_optic_layers(XrCompositionLayerQuad*out,unsigned capacity,unsigned&count){
 count=0;scopeViewingMask=0;
 if(!out||!capacity||fault||!begun||!visible||panel||scopePanel.active||overlayState.visible||!scopeSourceCaptureEnabled||!scopeSources.ready(frame,generation))return false;
 for(unsigned hand=0;hand<2;++hand){const GxScopeOptic&r=scopeOptics[hand];
  if(!gx_scope_optic::valid(r,frame,generation))continue;
  eye036::Matrix local{};gx_scope_pupil::Lens lens;GxPose lensPose{};
  if(!scope_local_lens(r,local,lens,lensPose))continue;
  if(gx_scope_optic::monocular(r)){if(scope_monocular_layer(hand,r,lens,out,capacity,count))scopeViewingMask|=1u<<hand;continue;}
  float mapping[2][12]{};bool visibleEye[2]{};unsigned needed=0;bool mappings=true;
  for(unsigned eye=0;eye<2;++eye){const auto eyeMatrix=pose075::pose(pose(rendered[eye].pose));
   gx_scope_eye::Axes axes;axes.right=gx_scope_eye::V(eyeMatrix[0],eyeMatrix[1],eyeMatrix[2]);
   axes.up=gx_scope_eye::V(eyeMatrix[4],eyeMatrix[5],eyeMatrix[6]);axes.forward=gx_scope_eye::V(-eyeMatrix[8],-eyeMatrix[9],-eyeMatrix[10]);
   const gx_scope_eye::V eyePosition(eyeMatrix[12],eyeMatrix[13],eyeMatrix[14]);
   visibleEye[eye]=gx_scope_pupil::enter(lens,eyePosition,axes.forward).visible;if(visibleEye[eye])++needed;
   gx_scope_eye::Fov fov;fov.left=rendered[eye].fov.angleLeft;fov.right=rendered[eye].fov.angleRight;
   fov.up=rendered[eye].fov.angleUp;fov.down=rendered[eye].fov.angleDown;gx_scope_eye::Mapping image;
   mappings=mappings&&gx_scope_eye::build(axes,fov,lens.axes,r.halfAngle,r.magnification,image)&&gx_scope_eye::constants(image,mapping[eye]);
  }
  // Never displace accepted HUD/modal layers or exceed runtime layer capacity.
  if(!needed||needed>capacity-count||!mappings)continue;
  if(!scopeCrop[hand].current(false,0,frame,generation)){
   if(!scopeCropAttempted[hand]){scopeCropAttempted[hand]=true;scopeCropReady[hand]=scopeCrop[hand].initialize(device.Get(),ScopeCropVs,sizeof(ScopeCropVs),ScopeReticlePs,sizeof(ScopeReticlePs),static_cast<DXGI_FORMAT>(selected_format));}
   if(!scopeCropReady[hand])continue;
   const GxScopeReticleBinding&binding=scopeReticleBindings[hand];unsigned alpha=0;ID3D11ShaderResourceView*reticle=nullptr;
   if(binding.frame==frame&&binding.generation==generation)reticle=scopeReticles.view(binding.reticleHash,frame,generation,alpha);
   if(!perf147.gpu(context.Get(),"scope_draw",int(0),int(hand),[&](){return scopeCrop[hand].draw(context.Get(),scopeSources,0,frame,generation,mapping[0],reticle,alpha);})||
      !perf147.gpu(context.Get(),"scope_draw",int(1),int(hand),[&](){return scopeCrop[hand].draw(context.Get(),scopeSources,1,frame,generation,mapping[1],reticle,alpha);})){health("scope_optical_draw");continue;}
   scopeLayerFrame[hand]=frame;scopeLayerPose[hand]=lensPose;
  }
  if(scopeLayerFrame[hand]!=frame)continue;
  for(unsigned eye=0;eye<2;++eye){if(!visibleEye[eye])continue;
   if(!scopeEye[hand][eye].swap)create_ui_eye(scopeEye[hand][eye],512,512);
   ID3D11Texture2D*target=acquire_ui_eye(scopeEye[hand][eye],512,512);
   perf147.gpu(context.Get(),"scope_copy",int(eye),int(hand),[&](){context->CopyResource(target,scopeCrop[hand].image(eye,frame,generation));});release_ui_eye(scopeEye[hand][eye]);
   XrCompositionLayerQuad&q=out[count++];q={XR_TYPE_COMPOSITION_LAYER_QUAD};q.space=space;
   q.eyeVisibility=eye?XR_EYE_VISIBILITY_RIGHT:XR_EYE_VISIBILITY_LEFT;
   q.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT|XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
   q.subImage.swapchain=scopeEye[hand][eye].swap;q.subImage.imageRect.extent={512,512};
   q.pose.orientation={float(lensPose.qx),float(lensPose.qy),float(lensPose.qz),float(lensPose.qw)};
   q.pose.position={float(lensPose.x),float(lensPose.y),float(lensPose.z)};q.size={float(r.radius*2),float(r.radius*2)};
  }
  scopeViewingMask|=1u<<hand;
 }if(count){scopeViewingFrame=frame;scopeViewingGeneration=generation;scopeViewingTick=GetTickCount64();}return count!=0;
}
bool scopeCropReady[2]{},scopeCropAttempted[2]{};
void read_scope_viewing(GxScopeViewing&out)const{
 out={};out.size=sizeof(out);out.version=1;
 const uint64_t now=GetTickCount64();
 if(fault||panel||scopePanel.active||overlayState.visible||!scopeSourceCaptureEnabled||!scopeViewingMask||
    !scopeViewingTick||now<scopeViewingTick||now-scopeViewingTick>250)return;
 out.frame=scopeViewingFrame;out.generation=scopeViewingGeneration;out.tick=scopeViewingTick;out.handMask=scopeViewingMask;
}
void invalidate_scope_optics(bool preserveViewing=false){scopeReticles.invalidate();for(unsigned hand=0;hand<2;++hand){scopeOptics[hand]=GxScopeOptic();scopeReticleBindings[hand]=GxScopeReticleBinding();scopeCrop[hand].invalidate();scopeLayerFrame[hand]=0;}
 if(!preserveViewing){scopeViewingMask=0;scopeViewingFrame=scopeViewingGeneration=scopeViewingTick=0;}}
void close_scope_optics(){invalidate_scope_optics();scopeReticles.close();for(unsigned hand=0;hand<2;++hand){for(unsigned eye=0;eye<2;++eye)close_ui_eye(scopeEye[hand][eye]);scopeCrop[hand].close();scopeCropReady[hand]=scopeCropAttempted[hand]=false;}}
