// Inside native_xr::Session. Publishes CheekyFoveatedDLSS's gaze snapshot (cheeky_gaze_abi.h) through GTAVXR's
// CheekyOpenXRLayer.dll shim once per submitted frame, in the place of Cheeky's own OpenXR layer:
//   - per-eye FOV (radians) and pose of the views this frame was rendered with;
//   - forward: the midpoint of both eye orientations projected into each eye (Cheeky's stereo_forward_pose), which is
//     what fuses fixed foveation on PSVR2's asymmetric views (AutoStereoAlignment);
//   - gaze: XR_EXT_eye_gaze_interaction projected into each eye, when the runtime reports a tracked gaze;
//   - resource identity: the D3D12 resource GTAVXR's DLSS SR writes for that eye (eye chain out12) and its full rect,
//     so Cheeky's exact_view_match pairs each snapshot view with the matching DLSS evaluation.
// The projection math is Cheeky's gaze_math::project_gaze_to_view.
uint64_t cheekySequence=0,cheekySessionGeneration=0;bool cheekyGazeLogged=false,cheekyViewsLogged=false;
static bool cheeky_project(const XrQuaternionf&dirQ,const XrPosef&view,const XrFovf&fov,float&u,float&v){
    const XrVector3f world=cheeky_rotate(dirQ,{0,0,-1});
    const XrQuaternionf& o=view.orientation;const XrVector3f d=cheeky_rotate({-o.x,-o.y,-o.z,o.w},world);
    if(!std::isfinite(d.x)||!std::isfinite(d.y)||!std::isfinite(d.z)||d.z>=-1e-4f)return false;
    const float l=std::tan(fov.angleLeft),r=std::tan(fov.angleRight),dn=std::tan(fov.angleDown),up=std::tan(fov.angleUp);
    if(!(r-l>1e-4f)||!(up-dn>1e-4f))return false;
    u=(d.x/-d.z-l)/(r-l);v=(up-d.y/-d.z)/(up-dn);
    return std::isfinite(u)&&std::isfinite(v);
}
void publish_cheeky_gaze(){
    const auto publish=gx_cheeky::publisher();if(!publish)return;
    if(!cheekySessionGeneration)cheekySessionGeneration=(uint64_t(GetCurrentProcessId())<<20)|(GetTickCount64()&0xFFFFF);
    gx_cheeky_abi::Snapshot s{};s.abi_version=gx_cheeky_abi::Version;s.structure_size=sizeof(s);s.view_count=2;
    s.sequence=++cheekySequence;LARGE_INTEGER q;QueryPerformanceCounter(&q);s.publication_qpc=uint64_t(q.QuadPart);
    s.predicted_display_time=s.sample_time=timing.predictedDisplayTime;s.session_generation=cheekySessionGeneration;s.swapchain_generation=1;
    strncpy_s(s.runtime_name,"SteamVR/OpenXR",_TRUNCATE);
    s.status_flags=gx_cheeky_abi::LayerActive;
    if(state==XR_SESSION_STATE_FOCUSED)s.status_flags|=gx_cheeky_abi::SessionFocused;
    if(actions.eyeGazeRequested)s.status_flags|=gx_cheeky_abi::ExtensionEnabled|gx_cheeky_abi::SystemSupported;
    // forward = normalised sum of the eye orientations (same hemisphere): the head's line of sight
    XrQuaternionf a=rendered[0].pose.orientation,b=rendered[1].pose.orientation;
    if(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w<0)b={-b.x,-b.y,-b.z,-b.w};
    XrQuaternionf fwd{a.x+b.x,a.y+b.y,a.z+b.z,a.w+b.w};
    const float fl=std::sqrt(fwd.x*fwd.x+fwd.y*fwd.y+fwd.z*fwd.z+fwd.w*fwd.w);
    const bool forwardOk=fl>1e-6f;if(forwardOk){fwd.x/=fl;fwd.y/=fl;fwd.z/=fl;fwd.w/=fl;}
    XrPosef gaze{};bool gazeActive=false;
    const bool gazeOk=actions.locate_gaze(space,timing.predictedDisplayTime,gaze,gazeActive);
    if(gazeActive)s.status_flags|=gx_cheeky_abi::ActionActive;
    bool resources=true,projected=gazeOk;
    for(unsigned e=0;e<2;++e){
        auto&v=s.views[e];v.structure_size=sizeof(v);v.view_index=e;
        v.flags=gx_cheeky_abi::ViewOrientationValid|gx_cheeky_abi::ViewPositionValid|gx_cheeky_abi::ViewOrientationTracked|
            gx_cheeky_abi::ViewPositionTracked|gx_cheeky_abi::ViewFovValid;
        const XrFovf&f=rendered[e].fov;v.fov_left=f.angleLeft;v.fov_right=f.angleRight;v.fov_up=f.angleUp;v.fov_down=f.angleDown;
        if(forwardOk&&cheeky_project(fwd,rendered[e].pose,f,v.forward_u,v.forward_v))v.flags|=gx_cheeky_abi::ViewForwardValid;
        else{v.forward_u=v.forward_v=.5f;}
        float gu=0,gv=0;
        if(gazeOk&&cheeky_project(gaze.orientation,rendered[e].pose,f,gu,gv)){v.center_u=gu;v.center_v=gv;}
        else{projected=false;v.center_u=v.forward_u;v.center_v=v.forward_v;}
        v.next_jump_u=v.center_u;v.next_jump_v=v.center_v;
        uint64_t id=0;uint32_t w=0,h=0;
        if(eyeChain.output_identity(e,id,w,h)){
            v.flags|=gx_cheeky_abi::ViewResourceValid;v.resource_identity=id;v.image_rect_width=w;v.image_rect_height=h;
            v.swapchain_identity=uint64_t(reinterpret_cast<uintptr_t>(eyes[e].swap));
        }else resources=false;
    }
    if(resources)s.status_flags|=gx_cheeky_abi::MappingReady;
    if(projected&&gazeActive)s.status_flags|=gx_cheeky_abi::GazeValid;
    publish(&s);
    if(!cheekyViewsLogged){cheekyViewsLogged=true;std::ostringstream o;o<<std::setprecision(4)<<"CHEEKY_GAZE first_snapshot mapping="<<(resources?1:0)
        <<" eye_gaze_ext="<<(actions.eyeGazeRequested?1:0)<<" bound="<<(actions.eyeGazeBound?1:0)
        <<" forward=" <<s.views[0].forward_u<<','<<s.views[0].forward_v<<" | "<<s.views[1].forward_u<<','<<s.views[1].forward_v
        <<" out="<<s.views[0].image_rect_width<<'x'<<s.views[0].image_rect_height;log(o.str());}
    if(!cheekyGazeLogged&&(s.status_flags&gx_cheeky_abi::GazeValid)){cheekyGazeLogged=true;log("CHEEKY_GAZE first valid eye gaze");}
}
