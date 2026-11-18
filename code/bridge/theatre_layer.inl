// Included in Session. Stereo theatre: for one frame the eye images show a screen fixed in the tracking
// space, textured per eye with the cutscene frame the game rendered for that eye. The frame is still an
// ordinary stereo projection frame: same swapchains, runtime poses and fields of view, no new layer type.
gx_theatre::Screen theatreScreen;
gx_theatre::Anchor theatreAnchor;
struct TheatreFrame {
    bool active=false, discard=false;
    gx_theatre::Plan plan;
    double aspect=16./9.;
};
TheatreFrame theatre;
uint64_t theatre_frames=0,theatreRetryFrame=0;
unsigned theatreFailures=0;uint32_t theatrePlacement=0;bool theatreLogged=false;
bool theatrePreviousWorld=false;
uint64_t theatreLastUsed=0;

void reset_theatre_frame(){
    theatre=TheatreFrame();
    if(theatreLastUsed&&frame-theatreLastUsed>600){
        theatreScreen.close();
        theatreAnchor.reset();
        theatreLastUsed=0;
    }
}

// A presentation or resource failure never faults the session: the caller goes back to its ordinary
// camera. Three failures in one session switch the theatre off until the next session.
void theatre_failed(const char*stage){
    theatre.active=false;++theatreFailures;
    const bool never=theatreFailures>=3;
    theatreRetryFrame=never?~uint64_t(0):frame+600;
    log(std::string("THEATRE_UNAVAILABLE stage=")+(stage?stage:"unknown")+" failures="+std::to_string(theatreFailures)+
        (never?" retry=next_session":" retry_frames=600"));
}
// GX_THEATRE_ARMED: views filled and this frame presents the screen. GX_THEATRE_REFUSED: nothing changed.
int compose_theatre(const GxFrame&f,const double*camera,const GxTheatre&r,GxViews&out){
    demand(f.size==sizeof(f)&&camera&&out.size==sizeof(out),"THEATRE_ABI");
    // A request for another frame cannot change this one's presentation.
    if(fault||!begun||copied||f.id!=frame||f.generation!=generation)return GX_THEATRE_REFUSED;
    theatre.active=false;
    if(!visible||panel||scopePanel.active||!f.renderable||frame<theatreRetryFrame||
       !gx_theatre::valid(r))return GX_THEATRE_REFUSED;
    const auto&a=rendered[0].pose.position;const auto&b=rendered[1].pose.position;
    // Tenths of a millimetre: tracking noise in the eye poses must not reach the filming pair.
    const double ipd=std::floor(std::sqrt(double(b.x-a.x)*(b.x-a.x)+double(b.y-a.y)*(b.y-a.y)+double(b.z-a.z)*(b.z-a.z))*1e4+.5)/1e4;
    const gx_theatre::Plan plan=gx_theatre::plan(r,width,height,ipd);
    if(!plan.valid)return GX_THEATRE_REFUSED;
    GxViews views=out;
    try{
        gx_theatre::compose(r,plan,camera,views);
        if(r.placement!=theatrePlacement){theatreAnchor.reset();theatrePlacement=r.placement;}
        if(!theatreAnchor.enter(pod(shared086::midpoint(pose(rendered[0].pose),pose(rendered[1].pose))),timing.predictedDisplayTime))
            return GX_THEATRE_REFUSED;
        theatreScreen.init(device.Get());
        if(theatreScreen.width()!=width||theatreScreen.height()!=height)
            theatreScreen.ensure(width,height,DXGI_FORMAT_B8G8R8A8_UNORM);
    }catch(const gx_chain::Error&e){theatre_failed(e.stage);health("theatre_arm");return GX_THEATRE_REFUSED;}
    catch(const std::runtime_error&){return GX_THEATRE_REFUSED;} // a camera or pose this frame cannot represent
    health("theatre_arm");
    out=views;theatre.active=true;theatre.plan=plan;theatre.aspect=r.aspect;theatreLastUsed=frame;
    if(!theatreLogged){theatreLogged=true;std::ostringstream s;s<<std::setprecision(5)<<"THEATRE_READY width="<<width<<" height="<<height<<
        " mips="<<(theatreScreen.mips()?1:0)<<" fov_degrees="<<r.vertical_fov_degrees<<" aspect="<<r.aspect<<" separation_m="<<plan.separation<<
        " crop="<<plan.uv[0]<<','<<plan.uv[1]<<','<<plan.uv[2]<<','<<plan.uv[3];log(s.str());}
    return GX_THEATRE_ARMED;
}
void theatre_present(unsigned e,uint64_t gen,ID3D11Texture2D*source,ID3D11Texture2D*target){
    D3D11_TEXTURE2D_DESC s{};source->GetDesc(&s);
    theatreScreen.ensure(width,height,s.Format);
    if(eyeChain.active())present_eye(e,gen,source,theatreScreen.image(e)); // NR/SR output at the swapchain size
    else theatreScreen.store(context.Get(),e,source);
    theatreScreen.filter(context.Get(),e);
    gx_theatre::Corners corners;
    const auto&v=rendered[e];
    const GxFov fov{v.fov.angleLeft,v.fov.angleRight,v.fov.angleUp,v.fov.angleDown};
    gx_chain::Need(theatreAnchor.valid()&&gx_theatre::corners(theatreAnchor.pose(),gx_theatre::ScreenWidth,
        gx_theatre::screen_height(theatre.aspect),pod(pose(v.pose)),fov,corners),"theatre_corners");
    theatreScreen.draw(context.Get(),target,e,corners,theatre.plan.uv);
}
// A failure can happen after the other eye's image was released. End this frame without
// layers so the runtime never displays two different presentation modes as a stereo pair.
void theatre_capture(unsigned e,uint64_t gen,ID3D11Texture2D*source,ID3D11Texture2D*target){
    try{theatre_present(e,gen,source,target);return;}
    catch(const gx_chain::Error&x){theatre_failed(x.stage);}
    catch(const std::runtime_error&x){theatre_failed(x.what());}
    theatre.discard=true;
    eyeChain.drop_pending(); // guides of this eye, if the failure came before present() consumed them
    eyeChain.invalidate_history();
    health("theatre_capture");
}
void close_theatre(){theatreScreen.close();theatreAnchor.reset();theatre=TheatreFrame();theatreRetryFrame=theatreLastUsed=0;theatreFailures=0;theatreLogged=theatrePreviousWorld=false;}
