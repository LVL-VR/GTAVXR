// Inside native_xr::Session. CheekyFoveatedDLSS's F11 overlay in the headset: the bridge takes the place of Cheeky's
// OpenXR layer as the consumer of its process-local menu ABI (shared/openxr_menu_bridge.hpp in the Cheeky fork):
//   CheekyOpenXRMenuAcquireFrame  - every world frame (keeps Cheeky's "OpenXR owns the headset menu" request fresh);
//                                   false while the menu is closed; else an AddRef'd D3D11 texture on the game device
//   CheekyOpenXRMenuCopy11        - Cheeky copies its texture into our swapchain image on its own context, under its lock
//   CheekyOpenXRMenuSetPointer    - controller ray hit on the panel in Cheeky display pixels (x, y, down, active)
//   CheekyOpenXRMenuReportStatus  - shown in Cheeky's menu status line
// Cheeky only prepares the headset texture while a module named CheekyOpenXRLayer.dll is resident; gx_cheeky loads it
// as a plain library (never as an OpenXR layer). While the panel shows, controller input is withheld from the game.
struct CheekyMenuFrame {uint32_t size,graphics_api,width,height,format;float display_width,display_height;void*device;void*queue;void*texture;};
using CheekyMenuAcquireFn=bool(__cdecl*)(CheekyMenuFrame*);
using CheekyMenuCopy11Fn=bool(__cdecl*)(void*,void*);
using CheekyMenuPointerFn=void(__cdecl*)(float,float,bool,bool);
using CheekyMenuReportFn=void(__cdecl*)(uint32_t);
enum {CHEEKY_MENU_SUBMITTED=0,CHEEKY_MENU_DEVICE_MISMATCH=2,CHEEKY_MENU_FORMAT_UNAVAILABLE=3,CHEEKY_MENU_SWAPCHAIN_FAILED=4,
      CHEEKY_MENU_IMAGE_FAILED=6,CHEEKY_MENU_COPY_FAILED=7};
Eye cheekyMenuEye;uint32_t cheekyMenuW=0,cheekyMenuH=0;int64_t cheekyMenuFormat=0;
bool cheekyMenuAnchored=false,cheekyMenuLogged=false;XrPosef cheekyMenuPose{};float cheekyMenuSize[2]{};
std::atomic<bool> cheekyMenuOpen{false};float cheekyTrigger[2]{};
CheekyMenuAcquireFn cheekyAcquire{};CheekyMenuCopy11Fn cheekyCopy{};CheekyMenuPointerFn cheekyPointer{};CheekyMenuReportFn cheekyReport{};
bool cheekyResolved=false;

static XrVector3f cheeky_rotate(const XrQuaternionf&q,const XrVector3f&v){
    const XrVector3f u{q.x,q.y,q.z};const float s=q.w;
    const float d=u.x*v.x+u.y*v.y+u.z*v.z,uu=u.x*u.x+u.y*u.y+u.z*u.z;
    const XrVector3f c{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
    return {2*d*u.x+(s*s-uu)*v.x+2*s*c.x,2*d*u.y+(s*s-uu)*v.y+2*s*c.y,2*d*u.z+(s*s-uu)*v.z+2*s*c.z};
}
// Same-family copies only: CopyResource needs an identical or typeless-compatible format.
static bool cheeky_copy_compatible(DXGI_FORMAT a,DXGI_FORMAT b){
    auto family=[](DXGI_FORMAT f){switch(f){
        case DXGI_FORMAT_R8G8B8A8_UNORM:case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:case DXGI_FORMAT_R8G8B8A8_TYPELESS:return 1;
        case DXGI_FORMAT_B8G8R8A8_UNORM:case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:case DXGI_FORMAT_B8G8R8A8_TYPELESS:return 2;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:case DXGI_FORMAT_R16G16B16A16_TYPELESS:return 3;
        case DXGI_FORMAT_R10G10B10A2_UNORM:case DXGI_FORMAT_R10G10B10A2_TYPELESS:return 4;default:return 0;}};
    return a==b||(family(a)&&family(a)==family(b));
}
bool cheeky_resolve(){
    if(cheekyResolved)return cheekyAcquire&&cheekyCopy;
    const HMODULE host=GetModuleHandleW(L"CheekyFoveatedDLSSHost.dll");if(!host)return false; // not started (yet)
    cheekyResolved=true;
    cheekyAcquire=reinterpret_cast<CheekyMenuAcquireFn>(GetProcAddress(host,"CheekyOpenXRMenuAcquireFrame"));
    cheekyCopy=reinterpret_cast<CheekyMenuCopy11Fn>(GetProcAddress(host,"CheekyOpenXRMenuCopy11"));
    cheekyPointer=reinterpret_cast<CheekyMenuPointerFn>(GetProcAddress(host,"CheekyOpenXRMenuSetPointer"));
    cheekyReport=reinterpret_cast<CheekyMenuReportFn>(GetProcAddress(host,"CheekyOpenXRMenuReportStatus"));
    log(std::string("CHEEKY_MENU consumer=")+(cheekyAcquire&&cheekyCopy?"ready":"exports_missing"));
    return cheekyAcquire&&cheekyCopy;
}
void cheeky_report(uint32_t s){if(cheekyReport)cheekyReport(s);}
void cheeky_menu_closed(){
    cheekyMenuOpen=false;cheekyMenuAnchored=false;
    if(cheekyPointer)cheekyPointer(0,0,false,false);
}
// Controller ray -> Cheeky display pixels. Right hand first, as Cheeky's own layer does.
void cheeky_menu_pointer(const CheekyMenuFrame&f){
    if(!cheekyPointer)return;
    GxPose aim[2]{};const uint32_t mask=actions.locate(space,timing.predictedDisplayTime,aim,true);
    const XrQuaternionf& q=cheekyMenuPose.orientation;const XrQuaternionf inv{-q.x,-q.y,-q.z,q.w};
    const XrVector3f n=cheeky_rotate(q,{0,0,1}),c=cheekyMenuPose.position;
    for(const unsigned h:{1u,0u}){
        if(!(mask&(1u<<h)))continue;
        const XrQuaternionf aq{float(aim[h].qx),float(aim[h].qy),float(aim[h].qz),float(aim[h].qw)};
        const XrVector3f p{float(aim[h].x),float(aim[h].y),float(aim[h].z)},d=cheeky_rotate(aq,{0,0,-1});
        const float den=d.x*n.x+d.y*n.y+d.z*n.z;if(std::fabs(den)<1e-4f)continue;
        const float t=((c.x-p.x)*n.x+(c.y-p.y)*n.y+(c.z-p.z)*n.z)/den;if(t<=0)continue;
        const XrVector3f hit{p.x+t*d.x-c.x,p.y+t*d.y-c.y,p.z+t*d.z-c.z},local=cheeky_rotate(inv,hit);
        const float u=local.x/cheekyMenuSize[0]+.5f,v=.5f-local.y/cheekyMenuSize[1];
        if(u<0||u>1||v<0||v>1)continue;
        cheekyPointer(u*f.display_width,v*f.display_height,cheekyTrigger[h]>.5f,true);return;
    }
    cheekyPointer(0,0,false,false);
}
// World frames only. Returns true with a filled quad when Cheeky's menu is open and its image reached our swapchain.
bool cheeky_menu_layer(XrCompositionLayerQuad&q){
    if(!cheeky_resolve())return false;
    CheekyMenuFrame f{};f.size=sizeof(f);
    if(!cheekyAcquire(reinterpret_cast<CheekyMenuFrame*>(&f))||!f.texture){if(cheekyMenuOpen)cheeky_menu_closed();return false;}
    auto*source=static_cast<ID3D11Texture2D*>(f.texture);
    struct Release{IUnknown*u;~Release(){u->Release();}} hold{source};
    if(f.graphics_api!=11||f.device!=device.Get()){cheeky_report(CHEEKY_MENU_DEVICE_MISMATCH);return false;}
    if(!f.width||!f.height||!f.display_width||!f.display_height)return false;
    if(cheekyMenuEye.swap&&(f.width!=cheekyMenuW||f.height!=cheekyMenuH))close_cheeky_menu();
    if(!cheekyMenuEye.swap){
        uint32_t n=0;if(XR_FAILED(api.EnumerateSwapchainFormats(session,0,&n,nullptr))||!n||n>256)return false;
        std::vector<int64_t> formats(n);if(XR_FAILED(api.EnumerateSwapchainFormats(session,n,&n,formats.data())))return false;
        cheekyMenuFormat=0;
        for(int64_t x:formats)if(x==int64_t(f.format)){cheekyMenuFormat=x;break;}
        if(!cheekyMenuFormat)for(int64_t x:formats)if(cheeky_copy_compatible(DXGI_FORMAT(x),DXGI_FORMAT(f.format))){cheekyMenuFormat=x;break;}
        if(!cheekyMenuFormat){cheeky_report(CHEEKY_MENU_FORMAT_UNAVAILABLE);return false;}
        XrSwapchainCreateInfo sw{XR_TYPE_SWAPCHAIN_CREATE_INFO};sw.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        sw.format=cheekyMenuFormat;sw.sampleCount=1;sw.width=f.width;sw.height=f.height;sw.faceCount=sw.arraySize=sw.mipCount=1;
        if(XR_FAILED(api.CreateSwapchain(session,&sw,&cheekyMenuEye.swap))){cheekyMenuEye.swap={};cheeky_report(CHEEKY_MENU_SWAPCHAIN_FAILED);return false;}
        n=0;check(api.EnumerateSwapchainImages(cheekyMenuEye.swap,0,&n,nullptr),"CheekyMenuImageCount");demand(n&&n<=16,"CHEEKY_MENU_IMAGE_COUNT");
        cheekyMenuEye.images.assign(n,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
        check(api.EnumerateSwapchainImages(cheekyMenuEye.swap,n,&n,reinterpret_cast<XrSwapchainImageBaseHeader*>(cheekyMenuEye.images.data())),"CheekyMenuImages");
        cheekyMenuW=f.width;cheekyMenuH=f.height;
        log("CHEEKY_MENU swapchain "+std::to_string(f.width)+"x"+std::to_string(f.height)+" source_format="+std::to_string(f.format)+
            " swap_format="+std::to_string(cheekyMenuFormat));
    }
    // Anchor in front of the current head pose when the menu opens (yaw only), 1.3 m away; stays put while open.
    if(!cheekyMenuAnchored){
        const GxPose head=pod(shared086::midpoint(pose(rendered[0].pose),pose(rendered[1].pose)));
        const XrQuaternionf hq{float(head.qx),float(head.qy),float(head.qz),float(head.qw)};
        XrVector3f fwd=cheeky_rotate(hq,{0,0,-1});fwd.y=0;const float len=std::sqrt(fwd.x*fwd.x+fwd.z*fwd.z);
        if(len<1e-3f)return false;fwd.x/=len;fwd.z/=len;
        const float yaw=std::atan2(-fwd.x,-fwd.z);
        cheekyMenuPose.orientation={0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};
        cheekyMenuPose.position={float(head.x)+fwd.x*1.3f,float(head.y)-.1f,float(head.z)+fwd.z*1.3f};
        cheekyMenuSize[0]=1.4f;cheekyMenuSize[1]=1.4f*f.display_height/f.display_width;
        cheekyMenuAnchored=true;
    }
    uint32_t index=0;XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    if(XR_FAILED(api.AcquireSwapchainImage(cheekyMenuEye.swap,&acquire,&index))){cheeky_report(CHEEKY_MENU_IMAGE_FAILED);return false;}
    cheekyMenuEye.acquired=true;
    XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=1000000000;
    demand(api.WaitSwapchainImage(cheekyMenuEye.swap,&wait)==XR_SUCCESS,"CHEEKY_MENU_WAIT");cheekyMenuEye.waited=true;
    demand(index<cheekyMenuEye.images.size()&&cheekyMenuEye.images[index].texture,"CHEEKY_MENU_INDEX");
    const bool menuCopied=cheekyCopy(cheekyMenuEye.images[index].texture,source);
    XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};check(api.ReleaseSwapchainImage(cheekyMenuEye.swap,&release),"CheekyMenuRelease");
    cheekyMenuEye.acquired=cheekyMenuEye.waited=false;
    if(!menuCopied){cheeky_report(CHEEKY_MENU_COPY_FAILED);return false;}
    cheekyMenuOpen=true;
    cheeky_menu_pointer(f);
    q.space=space;q.eyeVisibility=XR_EYE_VISIBILITY_BOTH;q.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT; // premultiplied
    q.subImage.swapchain=cheekyMenuEye.swap;q.subImage.imageRect.extent={int32_t(f.width),int32_t(f.height)};
    q.pose=cheekyMenuPose;q.size={cheekyMenuSize[0],cheekyMenuSize[1]};
    if(!cheekyMenuLogged){cheekyMenuLogged=true;log("CHEEKY_MENU first headset frame submitted");cheeky_report(CHEEKY_MENU_SUBMITTED);}
    return true;
}
// Called from read_input: remember the triggers for the ray click, then withhold controller input from the game.
void cheeky_menu_input(GxInput&out){
    for(unsigned h=0;h<2;++h)cheekyTrigger[h]=out.trigger[h];
    if(!cheekyMenuOpen)return;
    out.buttons=0;out.reserved&=~uint32_t(GX_INPUT_BOTH_CLICKS);
    for(unsigned h=0;h<2;++h){out.trigger[h]=0;out.grip[h]=0;out.stick[h][0]=out.stick[h][1]=0;}
}
void close_cheeky_menu(){
    if(cheekyMenuEye.acquired){
        if(!cheekyMenuEye.waited){XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=1000000000;api.WaitSwapchainImage(cheekyMenuEye.swap,&wait);}
        XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};api.ReleaseSwapchainImage(cheekyMenuEye.swap,&release);
        cheekyMenuEye.acquired=cheekyMenuEye.waited=false;
    }
    if(cheekyMenuEye.swap){cheekyMenuEye.images.clear();api.DestroySwapchain(cheekyMenuEye.swap);cheekyMenuEye.swap={};}
    cheekyMenuW=cheekyMenuH=0;cheekyMenuOpen=false;cheekyMenuAnchored=false;
}
