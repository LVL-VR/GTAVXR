#include "session.h"
#include "eye_query.h"
#include "first_image.h"
#include <memory>
#include <atomic>

namespace {
thread_local std::string error_text;
struct Bridge {
    native_xr::Session xr;
    HANDLE log=INVALID_HANDLE_VALUE;
    // build143 P2 (TWO_FRAMES): calls from DIFFERENT threads serialize briefly (update-thread input/rumble/scope-viewing
    // may now run without the engine D3D context while the render thread is inside a capture); same-thread reentry is
    // still refused. Lock order is always engine context -> bridge lock; the bridge never takes the engine context.
    SRWLOCK lock=SRWLOCK_INIT;
    std::atomic<DWORD> owner{0};
    ~Bridge(){if(log!=INVALID_HANDLE_VALUE)CloseHandle(log);}
    void write(const std::string&s) {
        auto line=s+"\r\n";DWORD n=0;
        if(!WriteFile(log,line.data(),DWORD(line.size()),&n,nullptr)||n!=line.size())throw std::runtime_error("XR_LOG_WRITE");
    }
};
template<class F> int guarded(void*h,F f,bool allow_fault=false,const char*perfPhase147=nullptr) noexcept {
    if(!h){error_text="NULL_BRIDGE";return 1;}auto&b=*static_cast<Bridge*>(h);
    if(b.owner.load()==GetCurrentThreadId()){error_text="REENTRANT_BRIDGE_CALL";return 1;}
    const long long lockStart147=perfPhase147?native_xr::Perf147::now():0;
    AcquireSRWLockExclusive(&b.lock);b.owner.store(GetCurrentThreadId());
    const long long lockEnd147=perfPhase147?native_xr::Perf147::now():0;
    int result=0;
    try{native_xr::demand(allow_fault||!b.xr.fault,"BRIDGE_FAULTED");f(b);}
    catch(const std::exception&e){
        error_text=e.what();b.xr.last_error=error_text;
        // Only the polled terminal event at a closed frame is restartable.
        // Partial frame, device, ownership, copy and arbitrary runtime errors stay fatal.
        const bool restart=b.xr.restartable_terminal(e.what());
        b.xr.fault=!restart;result=restart?3:1;
        try{b.write((restart?"SESSION_ENDED ":"FAULT ")+b.xr.last_error);}catch(...) {b.xr.fault=true;result=1;}
    }
    catch(...){b.xr.fault=true;b.xr.last_error=error_text="UNKNOWN_BRIDGE_ERROR";result=1;}
    if(perfPhase147&&b.xr.frame&&b.xr.generation&&(strcmp(perfPhase147,"begin")||b.xr.begun)){try{std::ostringstream s;s<<"event=PERF147_BRIDGE_LOCK phase="<<perfPhase147<<" frame="<<b.xr.frame<<" generation="<<b.xr.generation<<" lock_ms="<<std::fixed<<std::setprecision(6)<<b.xr.perf147.ms(lockEnd147-lockStart147)<<" result="<<result;b.xr.perf147.emit(s.str());}catch(...){}}
    b.owner.store(0);ReleaseSRWLockExclusive(&b.lock);return result;
}
int GX_CALL create(const GxConfig*c,void**out) noexcept {
    if(!out){error_text="NULL_CREATE_OUTPUT";return 1;}*out=nullptr;
    try {
        native_xr::demand(c&&c->size==sizeof(*c)&&!c->reserved&&c->log_path,"CREATE_CONFIG");
        auto b=std::make_unique<Bridge>();
        b->log=CreateFileW(c->log_path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        native_xr::demand(b->log!=INVALID_HANDLE_VALUE,"EXCLUSIVE_LOG_CREATE");
        // Return the failed partially initialized handle too, so cleanup never loses ownership.
        auto*p=b.release();*out=p;p->xr.log=[p](const std::string&s){p->write(s);};
        const std::wstring path(c->log_path);const auto directory=path.substr(0,path.find_last_of(L"/\\"));
        wchar_t play[8]={},diagnostic[8]={};
        GetEnvironmentVariableW(L"GTAVXR_USER_PLAY",play,8);GetEnvironmentVariableW(L"GTAVXR_DIAGNOSTIC",diagnostic,8);
        p->xr.compact_logging=!wcscmp(play,L"1")&&wcscmp(diagnostic,L"1");
        // 167-neural B2: NGX writes its own logs next to ours; the forwarder copies sit next to this DLL
        p->xr.chainDataDir=directory;{HMODULE self=nullptr;wchar_t module[MAX_PATH]={};
            if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&create),&self)&&
               GetModuleFileNameW(self,module,MAX_PATH)<MAX_PATH){std::wstring m(module);p->xr.chainBridgeDir=m.substr(0,m.find_last_of(L"/\\"));}}
        p->xr.perf147.init(false,p->xr.log); // build148: perf telemetry OFF (user closed the performance section)
        if(!p->xr.compact_logging)p->xr.first_image=[p,directory](ID3D11Texture2D*t,unsigned e){native_xr::save_first_image(p->xr,t,e,directory);};
        return guarded(p,[&](Bridge&v){v.xr.initialize(static_cast<ID3D11Device*>(c->d3d_device),c->width,c->height,c->loader_path);});
    }catch(const std::exception&e){error_text=e.what();return 1;}catch(...){error_text="CREATE_EXCEPTION";return 1;}
}
int GX_CALL begin(void*h,uint64_t generation,GxFrame*out) noexcept {
    return guarded(h,[&](Bridge&b){native_xr::demand(out&&out->size==sizeof(*out),"FRAME_OUTPUT_SIZE");*out=b.xr.begin(generation);},false,"begin");
}
int GX_CALL compose(const GxFrame*f,const double*camera,double n,double z,GxViews*out) noexcept {
    try{native_xr::demand(f&&out,"COMPOSE_POINTER");native_xr::compose(*f,camera,n,z,*out);return 0;}
    catch(const std::exception&e){error_text=e.what();return native_xr::transient_compose_error(e.what())?2:1;}catch(...){error_text="COMPOSE_EXCEPTION";return 1;}
}
int GX_CALL capture(void*h,uint64_t f,uint64_t g,uint32_t e,void*t) noexcept {
    return guarded(h,[&](Bridge&b){b.xr.capture(f,g,e,static_cast<ID3D11Texture2D*>(t));});
}
int GX_CALL finish(void*h,uint64_t f,uint64_t g) noexcept {return guarded(h,[&](Bridge&b){b.xr.finish(f,g);},false,"finish");}
int GX_CALL cancel(void*h) noexcept {return guarded(h,[](Bridge&b){b.xr.finish(0,0,true);},true);}
int GX_CALL destroy(void*h) noexcept {
    if(!h)return 0;
    auto rc=guarded(h,[](Bridge&b){b.xr.close();},true);
    if(!rc)delete static_cast<Bridge*>(h); // retain ownership on cleanup failure; do not unload live runtime.
    return rc;
}
const char* GX_CALL error(void*h) noexcept {
    if(h&&!static_cast<Bridge*>(h)->xr.last_error.empty())return static_cast<Bridge*>(h)->xr.last_error.c_str();
    return error_text.c_str();
}
}
extern "C" __declspec(dllexport) int GX_CALL GTAVXR_GetOpenXR(uint32_t version,uint32_t size,GxApi*out) noexcept {
    if(version!=1||size!=sizeof(GxApi)||!out)return 1;
    *out={sizeof(GxApi),1,create,begin,compose,capture,finish,cancel,destroy,error};return 0;
}
extern "C" __declspec(dllexport) int GX_CALL GTAVXR_ReadInput(void*h,GxInput*out) noexcept {
    return guarded(h,[&](Bridge&b){
        native_xr::demand(out&&out->size==sizeof(*out)&&out->version==1,"INPUT_ABI");
        *out=b.xr.read_input();
    });
}
// build165 item 18: the runtime floor (STAGE); valid 0 = no floor (the EXE falls back to the countdown).
extern "C" __declspec(dllexport) int GX_CALL GtavxrReadFloor1(void*h,GxFloor*out) noexcept {
    return guarded(h,[&](Bridge&b){
        native_xr::demand(out&&out->size==sizeof(*out)&&out->version==1,"FLOOR_ABI");
        double y=0;const bool ok=b.xr.read_floor(y);out->valid=ok?1u:0u;out->local_above_floor=ok?y:0.0;
    });
}
extern "C" __declspec(dllexport) int GX_CALL GTAVXR_SetNativeRumble(void*h,const GxRumble*state) noexcept {
    return guarded(h,[&](Bridge&b){native_xr::demand(state,"RUMBLE_POINTER");b.xr.set_native_rumble(*state);});
}
extern "C" __declspec(dllexport) int GX_CALL GTAVXR_SetMenuPanel(void*h) noexcept {
    bool accepted=false;
    const int result=guarded(h,[&](Bridge&b){accepted=b.xr.set_menu_panel();});
    return result?result:(accepted?0:1);
}
extern "C" __declspec(dllexport) int GX_CALL GTAVXR_SetMenuAppearance(void*h,const menyoo_vr::Appearance*state) noexcept {
    return guarded(h,[&](Bridge&b){native_xr::demand(state,"MENU_APPEARANCE_POINTER");b.xr.set_menu_appearance(*state);});
}
extern "C" __declspec(dllexport) int GX_CALL GTAVXR_SetOverlay(void*h,const GxOverlay*state) noexcept {
    return guarded(h,[&](Bridge&b){native_xr::demand(state,"OVERLAY_POINTER");b.xr.set_overlay(*state);});
}
extern "C" __declspec(dllexport) int GX_CALL GTAVXR_SetWristProfiles(void*h,const GxWristProfiles*state) noexcept {
    return guarded(h,[&](Bridge&b){native_xr::demand(state,"WRIST_PROFILE_POINTER");b.xr.set_wrist_profiles(*state);});
}
extern "C" __declspec(dllexport) int GX_CALL GTAVXR_SetUiElementProfiles(void*h,const GxUiElementProfiles*state) noexcept {
    return guarded(h,[&](Bridge&b){native_xr::demand(state,"UI_ELEMENT_PROFILE_POINTER");b.xr.set_ui_element_profiles(*state);});
}
// build167 items 1/2/3: where each HUD panel is drawn (anchor, offset, scale, face the head) + the vehicle anchor pose;
// ABI 2 (source-v16) adds the wrist buttons card, so the export name changed with the struct
extern "C" __declspec(dllexport) int GX_CALL GtavxrSetPanelPlacements2(void*h,const GxPanelPlacements*state) noexcept {
    return guarded(h,[&](Bridge&b){native_xr::demand(state,"PANEL_PLACEMENT_POINTER");b.xr.set_panel_placements(*state);});
}
// build167 item 4: the head inside a wall, 0 = clear .. 1 = black world (sent every frame while dark)
extern "C" __declspec(dllexport) int GX_CALL GtavxrSetViewFade1(void*h,float alpha) noexcept {
    return guarded(h,[&](Bridge&b){b.xr.set_view_fade(alpha);});
}
extern "C" __declspec(dllexport) int GX_CALL GTAVXR_SetUiVisibility(void*h,const GxUiVisibility*state) noexcept {
    return guarded(h,[&](Bridge&b){native_xr::demand(state,"UI_VISIBILITY_POINTER");b.xr.set_ui_visibility(*state);});
}
extern "C" __declspec(dllexport) int GX_CALL GTAVXR_CaptureWrist(void*h,const GxWristCapture*state,void*source) noexcept {
    return guarded(h,[&](Bridge&b){native_xr::demand(state,"WRIST_POINTER");b.xr.capture_wrist(*state,static_cast<ID3D11Texture2D*>(source));});
}
extern "C" __declspec(dllexport) int GX_CALL GTAVXR_CaptureWeaponHud(void*h,const GxWristCapture*state,void*source) noexcept {
    return guarded(h,[&](Bridge&b){native_xr::demand(state,"WEAPON_HUD_POINTER");b.xr.capture_weapon_hud(*state,static_cast<ID3D11Texture2D*>(source));});
}
extern "C" __declspec(dllexport) int GX_CALL GTAVXR_CaptureNativeUi(void*h,const GxUiCapture*state,void*source) noexcept {
    int delivery=GX_UI_CAPTURE_REJECTED;
    const int result=guarded(h,[&](Bridge&b){native_xr::demand(state,"UI_POINTER");
        delivery=b.xr.capture_native_ui(*state,static_cast<ID3D11Texture2D*>(source))?GX_UI_CAPTURE_OK:GX_UI_CAPTURE_REJECTED;});
    return result?result:delivery;
}
extern "C" __declspec(dllexport) int GX_CALL GTAVXR_LocateHands(void*h,const GxFrame*f,const double*camera,GxHandTargets*out) noexcept {
    return guarded(h,[&](Bridge&b){native_xr::demand(f&&out,"HANDS_POINTER");b.xr.locate_hands(*f,camera,*out);});
}
extern "C" __declspec(dllexport) int GX_CALL GTAVXR_LocateAim(void*h,const GxFrame*f,const double*camera,GxHandTargets*out) noexcept {
    return guarded(h,[&](Bridge&b){native_xr::demand(f&&out,"AIM_POINTER");b.xr.locate_aim(*f,camera,*out);});
}

// build133: one-shot runtime per-eye size for the engine's scene size, before any XR session exists.
extern "C" __declspec(dllexport) int GX_CALL GtavxrQueryEyeSize1(const wchar_t*loader,const GxEyeSizeRequest*r,GxEyeSizeResult*out) noexcept {
    if(!r||!out||r->size!=sizeof(GxEyeSizeRequest)||r->version!=1||out->size!=sizeof(GxEyeSizeResult)||out->version!=1)return GX_EYE_ABI;
    try{
        OpenXrApi api;if(!loader||!*loader||!api.Load(loader)){out->status=GX_EYE_LOADER;return GX_EYE_LOADER;}
        const uint32_t status=native_xr::query_eye_size(api,*r,*out);api.Unload();return int(status);
    }catch(...){out->status=GX_EYE_EXCEPTION;return GX_EYE_EXCEPTION;}
}

extern "C" __declspec(dllexport) int GX_CALL GtavxrEnableScopeSource1(void*h,uint32_t enabled) noexcept {
 if(enabled>1)return 1;
 return guarded(h,[&](Bridge&b){b.xr.scopeSourceCaptureEnabled=enabled!=0;b.xr.scopeSources.invalidate();});
}
extern "C" __declspec(dllexport) int GX_CALL GtavxrCaptureScopeSource1(void*h,const GxScopeSourceCapture*r,void*source) noexcept {
 if(!r||!source)return 1;bool accepted=false;
 const int status=guarded(h,[&](Bridge&b){accepted=b.xr.capture_scope_source(*r,static_cast<ID3D11Texture2D*>(source));});
 return status?status:(accepted?0:1);
}

extern "C" __declspec(dllexport) int GX_CALL GtavxrSetScopeOptic1(void*h,const GxScopeOptic*r) noexcept {
 if(!r)return 1;bool accepted=false;
 const int status=guarded(h,[&](Bridge&b){accepted=b.xr.set_scope_optic(*r);});
 return status?status:(accepted?0:1);
}

extern "C" __declspec(dllexport) int GX_CALL GtavxrReadScopeViewing1(void*h,GxScopeViewing*out) noexcept {
 if(!out||out->size!=sizeof(*out)||out->version!=1)return 1;
 return guarded(h,[&](Bridge&b){b.xr.read_scope_viewing(*out);});
}

extern "C" __declspec(dllexport) int GX_CALL GtavxrCaptureScopeReticle1(void*h,const GxScopeReticleCapture*r,void*source) noexcept {
 if(!r||!source)return 1;bool accepted=false;
 const int result=guarded(h,[&](Bridge&b){accepted=b.xr.capture_scope_reticle(*r,static_cast<ID3D11Texture2D*>(source));});
 return result?result:(accepted?0:1);
}
extern "C" __declspec(dllexport) int GX_CALL GtavxrBindScopeReticle1(void*h,const GxScopeReticleBinding*r) noexcept {
 if(!r)return 1;bool accepted=false;
 const int result=guarded(h,[&](Bridge&b){accepted=b.xr.bind_scope_reticle(*r);});
 return result?result:(accepted?0:1);
}

// 167-neural B2 (eye_chain.h ABI): the per-eye NR/SR chain. Configure: 0 = NR/SR ready, 1 = runs as the bilinear blit
// (reason in the log and the status), -1 = error (the EXE faults). Guides/params/status: 0 ok, 1 = no chain, -1 = error.
extern "C" __declspec(dllexport) int GX_CALL GtavxrConfigureEyeChain1(void*h,const GxEyeChainConfig*k) noexcept {
    int state=-1;
    const int r=guarded(h,[&](Bridge&b){native_xr::demand(k&&k->size==sizeof(*k)&&k->version==1,"EYE_CHAIN_ABI");state=b.xr.configure_eye_chain(*k);});
    return r?-1:state;
}
extern "C" __declspec(dllexport) int GX_CALL GtavxrCaptureEyeGuides1(void*h,const GxEyeGuides*g,void*depth) noexcept {
    bool active=false;
    const int r=guarded(h,[&](Bridge&b){active=b.xr.eye_chain_guides(g,static_cast<ID3D11Resource*>(depth));});
    return r?-1:(active?0:1);
}
extern "C" __declspec(dllexport) int GX_CALL GtavxrSetNeuralParams1(void*h,uint32_t enabled,uint32_t intensity,uint32_t compare) noexcept {
    bool active=false;
    const int r=guarded(h,[&](Bridge&b){active=b.xr.set_neural_params(enabled,intensity,compare);});
    return r?-1:(active?0:1);
}
extern "C" __declspec(dllexport) int GX_CALL GtavxrReadEyeChainStatus1(void*h,GxEyeChainStatus*out) noexcept {
    if(!out)return -1;
    const int r=guarded(h,[&](Bridge&b){b.xr.eyeChain.status(*out);},true);
    return r?-1:0;
}

// Optional native flat scope presentation. Ordinary ABI/order/conflict rejection
// returns a refusal without throwing through guarded() and faulting the session.
// build143 P2: engine feature probe. bit0 = cross-thread calls serialize (guarded) instead of being refused, so the
// update thread may read input/rumble/scope viewing without the engine D3D context while the render thread captures.
extern "C" __declspec(dllexport) int GX_CALL GtavxrBridgeFeatures1(uint32_t*mask) noexcept {if(!mask)return 1;*mask=1u;return 0;}
extern "C" __declspec(dllexport) int GX_CALL GtavxrSetScopePanel1(void*h,const GxScopePanel*p) noexcept {
    bool accepted=false;
    const int result=guarded(h,[&](Bridge&b){accepted=b.xr.set_scope_panel(p);});
    return result?result:(accepted?0:1);
}
