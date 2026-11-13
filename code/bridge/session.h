#pragma once
// Narrow descendant of Task075 session; incorporates078/080/123 proven corrections.
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#define XR_NO_PROTOTYPES
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <algorithm>
#include <array>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include "frame.h"
#include "native_frustum.h"
#include "readiness.h"
#include "xr_api004.h"
#include "bridge.h"
#include "actions.h"
#include "hands.h"
#include "menu_bitmap.h"
#include "menu_style_bitmap.h"
#include "appearance_bridge.h"
#include "wrist.h"
#include "scope_source_cache.h"
#include "scope_source_api.h"
#include "scope_crop_gpu.h"
#include "scope_optic_math.h"
#include "scope_pupil.h"
#include "scope_crop_vs_bytes.h"
#include "scope_crop_ps_bytes.h"
#include "scope_reticle.h"
#include "scope_reticle_ps_bytes.h"
#include "native_ui.h"
#include "hand_display_policy.h"
#include "wrist_calibration.h"
#include "panel_placement.h" // build167 items 1/2
#include "haptic_owner_gate.h"
#include "wanted_ui_policy.h"
#include "menu_panel_policy.h"
#include "ui_element_policy.h"
#include "ui_visibility.h"
#include "pause_anchor.h"
#include "scope_panel_policy.h"

#include "perf147_bridge.h"
#include "eye_chain_gpu.h" // 167-neural A2/B2

namespace native_xr {
using Microsoft::WRL::ComPtr;
using pose075::demand;
inline void check(XrResult r,const char* name) {
    if(XR_FAILED(r)) throw std::runtime_error(std::string(name)+":"+std::to_string(r));
}
inline pose075::Pose pose(const XrPosef&p) {
    return {p.position.x,p.position.y,p.position.z,p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w};
}
inline GxPose pod(const pose075::Pose&p) {return {p.x,p.y,p.z,p.qx,p.qy,p.qz,p.qw};}
inline pose075::Pose pose(const GxPose&p) {return {p.x,p.y,p.z,p.qx,p.qy,p.qz,p.qw};}
inline void compose(const GxFrame&f,const double* camera,double near_z,double far_z,GxViews&out) {
    demand(f.size==sizeof(f)&&f.renderable&&f.id&&f.generation&&camera&&out.size==sizeof(out),"COMPOSE_IDENTITY");
    eye036::Matrix base{}; std::copy_n(camera,16,base.begin());
    std::array<pose075::Pose,2> eyes{pose(f.eyes[0]),pose(f.eyes[1])};
    auto reference=pose(f.reference);
    for(unsigned e=0;e<2;++e)demand(gx_pose::validate(eyes[e])!=gx_pose::Rejected,"XR_POSE_REJECTED");
    demand(gx_pose::validate(reference)!=gx_pose::Rejected,"XR_POSE_REJECTED");
    std::array<pose075::Fov,2> fov;
    for(unsigned e=0;e<2;++e) fov[e]={f.fov[e].left,f.fov[e].right,f.fov[e].up,f.fov[e].down};
    const shared086::Clip clip{near_z,far_z};
    auto v=shared086::make({base,reference},{eye036::inverse(base),shared086::clip_projection(clip)},eyes,fov,clip,16./9.);
    v.source_frustum=native_envelope(v.center_vi,v.eyes,clip,16./9.);
    demand(v.source_frustum.representable,"CENTER_FRUSTUM_UNREPRESENTABLE");
    std::copy(v.center_vi.begin(),v.center_vi.end(),out.center);
    for(unsigned e=0;e<2;++e) {
        std::copy(v.eyes[e].vi.begin(),v.eyes[e].vi.end(),out.camera[e]);
        std::copy(v.eyes[e].projection.begin(),v.eyes[e].projection.end(),out.projection[e]);
    }
    out.center_fov_degrees=v.source_frustum.vertical_degrees;
    out.center_aspect=v.source_frustum.aspect;out.center_near=v.source_frustum.near_z;out.center_far=v.source_frustum.far_z;
}
inline void hand_world(const GxFrame&f,const double*camera,GxHandTargets&out){
    eye036::Matrix base{};std::copy_n(camera,16,base.begin());reference084::rigid(base);
    const auto mapping=eye036::mul(eye036::inverse(reference084::xr_reference(pose(f.reference))),reference084::level(base,{0,0,1}));
    for(unsigned h=0;h<2;++h)if(out.valid_mask&(1u<<h)){
        const auto world=eye036::mul(pose075::pose(pose(out.local[h])),mapping);
        std::copy(world.begin(),world.end(),out.world[h]);
    }
}
struct Eye {XrSwapchain swap{};std::vector<XrSwapchainImageD3D11KHR> images;bool acquired=false,waited=false;};
struct Session {
    OpenXrApi api;
    XrInstance instance{};XrSystemId system{};XrSession session{};XrSpace space{};
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Perf147 perf147;
    std::array<Eye,2> eyes;
    std::array<XrView,2> rendered{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};
    XrFrameState timing{XR_TYPE_FRAME_STATE};XrSessionState state=XR_SESSION_STATE_UNKNOWN;
    ready078::Gate startup;pose075::Pose reference;Actions actions;
    GxRumble nativeRumble{};uint64_t rumbleTick=0;
    gx_haptic::ImmediateOwnerGate spatialOwnerGate; // bridge-private, never crosses ABI
    XrSpace head_space{};bool panel=false;uint64_t menu_frames=0;
    XrSpace stage_space{};PFN_xrLocateSpace locate_floor{}; // build165 item 18: optional runtime floor (STAGE)
    gx_pause_anchor::Anchor pauseAnchor;
    XrTime pauseSpaceChange=0;
    bool running=false,anchored=false,begun=false,visible=false,terminal=false,fault=false;
    bool poseRenormalizedLogged=false;uint64_t poseRejected=0;
    uint32_t width=0,height=0,copied=0;uint64_t frame=0,generation=0,submitted=0,last_pose_tick=0;
    // build133: eye swapchains follow the runtime-driven scene size (GxConfig width/height); the pause panel
    // keeps the window-sized UI image in its own lazily created swapchain. Receipts are sparse.
    Eye panelEye;uint32_t panelWidth=0,panelHeight=0;bool fovLogged=false;
    LONGLONG timingLast=0,timingFrequency=0;uint64_t timingFrames=0,timingLate=0;double timingSumMs=0,timingMaxMs=0;
    int64_t selected_format=0;std::string last_error;
    // 167-neural A2/B2: RGBA swapchain fallback (copies become draws) and the per-eye NR/SR chain
    bool swapRgba=false;gx_chain::Blitter swapBlit;gx_chain::Chain eyeChain;std::wstring chainDataDir,chainBridgeDir;
    std::function<void(const std::string&)> log=[](const std::string&){};
    std::function<void(ID3D11Texture2D*,unsigned)> first_image;
    bool compact_logging=false; // user play: sparse receipts, no automatic readback images.
    GxWristProfiles wristProfiles=gx_wrist::defaults();
    void set_wrist_profiles(const GxWristProfiles&p){demand(gx_wrist::valid(p),"WRIST_PROFILE_ABI");wristProfiles=p;}
    GxUiElementProfiles uiElementProfiles=gx_ui_element::defaults();
    // build167 items 1/2: where each HUD panel is drawn (all DEFAULT = source-v14), the last head pose for "face the head"
    GxPanelPlacements panelPlacements=gx_panel::defaults();GxPose panelHead{};bool panelHeadValid=false;
    void set_panel_placements(const GxPanelPlacements&p){demand(gx_panel::valid(p),"PANEL_PLACEMENT_ABI");panelPlacements=p;}
    bool panel_free(unsigned k)const{if(k>=gx_panel::COUNT)return false;const unsigned a=panelPlacements.panel[k].anchor;
        return a==gx_panel::HEAD||a==gx_panel::ROOM||a==gx_panel::VEHICLE;}
    // the placed pose of panel k (DEFAULT: unchanged); false = not shown this frame (its anchor is not tracked)
    bool place_panel(XrCompositionLayerQuad&q,unsigned k){
        if(k>=gx_panel::COUNT)return true;const GxPanelPlace&e=panelPlacements.panel[k];if(e.anchor==gx_panel::DEFAULT)return true;
        GxPose p=e.offset;
        if(e.anchor==gx_panel::HEAD){if(!head_space)return false;q.space=head_space;}
        else{GxPose base=gx_panel::identity();
            if(e.anchor==gx_panel::LEFT_HAND||e.anchor==gx_panel::RIGHT_HAND){const unsigned h=e.anchor==gx_panel::RIGHT_HAND?1u:0u;
                if(hudHandFrame!=frame||!(hudHandMask&(1u<<h)))return false;base=hudGrip[h];}
            else if(e.anchor==gx_panel::VEHICLE){if(!panelPlacements.vehicleValid)return false;base=panelPlacements.vehicle;}
            p=gx_panel::compose(base,e.offset);if(e.face&&panelHeadValid)p=gx_panel::faceHead(p,panelHead);q.space=space;}
        q.pose.orientation={float(p.qx),float(p.qy),float(p.qz),float(p.qw)};q.pose.position={float(p.x),float(p.y),float(p.z)};
        q.size.width*=e.scale;q.size.height*=e.scale;return true;
    }
    GxUiVisibility uiVisibility=gx_ui_visibility::defaults();
    void set_ui_visibility(const GxUiVisibility&p){demand(gx_ui_visibility::valid(p),"UI_VISIBILITY_ABI");
        if(memcmp(&p,&uiVisibility,sizeof(p))){uiVisibility=p;rightPlanFrame=rightUiFrame=elementUiFrame=0;}}
    void set_ui_element_profiles(const GxUiElementProfiles&p){demand(gx_ui_element::valid(p),"UI_ELEMENT_PROFILE_ABI");
        if(!gx_ui_element::same(p,uiElementProfiles)){uiElementProfiles=p;rightPlanFrame=rightUiFrame=elementUiFrame=0;}}
#include "menu_layer.inl"
#include "wrist_layer.inl"
#include "weapon_hud_layer.inl"
#include "native_ui_layer.inl"
#include "hand_context_layer.inl"
#include "buttons_layer.inl" // build167 item 3
#include "fade_layer.inl" // build167 item 4
#include "scope_optic_layer.inl"
#include "scope_panel_layer.inl"
#include "eye_chain_session.inl" // 167-neural A2/B2

    void health(const char*boundary) {
        demand(device.Get()!=nullptr,"XR_NO_DEVICE");
        const HRESULT reason=device->GetDeviceRemovedReason();
        if(frame<8||FAILED(reason)){
            std::ostringstream s;s<<"DEVICE boundary="<<boundary<<" frame="<<frame
                <<" thread="<<GetCurrentThreadId()<<" flags=0x"<<std::hex<<device->GetCreationFlags()
                <<" reason=0x"<<uint32_t(reason);log(s.str());
        }
        if(FAILED(reason)){
            std::ostringstream s;s<<"XR_DEVICE_REMOVED boundary="<<boundary<<" reason=0x"<<std::hex<<uint32_t(reason);
            throw std::runtime_error(s.str());
        }
    }

    void initialize(ID3D11Device*d,uint32_t w,uint32_t h,const wchar_t*loader) {
        demand(d&&w&&h&&w<=4096&&h<=4096&&!instance,"XR_INITIALIZE_INPUT");
        device=d;d->GetImmediateContext(&context);width=w;height=h;
        demand(!(d->GetCreationFlags()&D3D11_CREATE_DEVICE_SINGLETHREADED),"XR_SINGLETHREADED_DEVICE_UNSUPPORTED");
        health("initialize_before");
        demand(loader&&*loader&&api.Load(loader),"XR_LOADER");
        const char* extension=XR_KHR_D3D11_ENABLE_EXTENSION_NAME;
        XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
        strcpy_s(ci.applicationInfo.applicationName,"GTAVXR Native");strcpy_s(ci.applicationInfo.engineName,"GTAVXR");
        ci.applicationInfo.apiVersion=XR_CURRENT_API_VERSION;ci.enabledExtensionCount=1;ci.enabledExtensionNames=&extension;
        check(api.CreateInstance(&ci,&instance),"CreateInstance");demand(api.LoadForInstance(instance),"XR_ENTRYPOINTS");
        XrSystemGetInfo si{XR_TYPE_SYSTEM_GET_INFO};si.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        check(api.GetSystem(instance,&si,&system),"GetSystem");
        XrSystemProperties uiProperties{XR_TYPE_SYSTEM_PROPERTIES};
        check(api.GetSystemProperties(instance,system,&uiProperties),"UiSystemProperties");
        uiMaxLayers=(std::min)(uiProperties.graphicsProperties.maxLayerCount,16u);
        uiMaxWidth=uiProperties.graphicsProperties.maxSwapchainImageWidth;uiMaxHeight=uiProperties.graphicsProperties.maxSwapchainImageHeight;
        demand(uiMaxLayers&&uiMaxWidth&&uiMaxHeight,"XR_UI_CAPABILITIES");
        XrGraphicsRequirementsD3D11KHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
        check(api.GetD3D11GraphicsRequirementsKHR(instance,system,&requirements),"GraphicsRequirements");
        ComPtr<IDXGIDevice> dx;ComPtr<IDXGIAdapter> adapter;DXGI_ADAPTER_DESC desc{};
        demand(SUCCEEDED(d->QueryInterface(IID_PPV_ARGS(&dx)))&&SUCCEEDED(dx->GetAdapter(&adapter))&&SUCCEEDED(adapter->GetDesc(&desc)),"XR_D3D_ADAPTER");
        demand(desc.AdapterLuid.HighPart==requirements.adapterLuid.HighPart&&desc.AdapterLuid.LowPart==requirements.adapterLuid.LowPart&&d->GetFeatureLevel()>=requirements.minFeatureLevel,"XR_ADAPTER_OR_FEATURE");
        uint32_t n=0;check(api.EnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&n,nullptr),"ViewCount");
        demand(n==2,"XR_STEREO_COUNT");std::array<XrViewConfigurationView,2> configs{{{XR_TYPE_VIEW_CONFIGURATION_VIEW},{XR_TYPE_VIEW_CONFIGURATION_VIEW}}};
        check(api.EnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,2,&n,configs.data()),"ViewConfigs");
        check(api.EnumerateEnvironmentBlendModes(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&n,nullptr),"BlendCount");
        demand(n&&n<=16,"XR_BLEND_COUNT");std::vector<XrEnvironmentBlendMode> blends(n);
        check(api.EnumerateEnvironmentBlendModes(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,n,&n,blends.data()),"BlendModes");
        demand(std::find(blends.begin(),blends.end(),XR_ENVIRONMENT_BLEND_MODE_OPAQUE)!=blends.end(),"XR_OPAQUE");
        XrGraphicsBindingD3D11KHR graphics{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};graphics.device=d;
        XrSessionCreateInfo sc{XR_TYPE_SESSION_CREATE_INFO};sc.systemId=system;sc.next=&graphics;check(api.CreateSession(instance,&sc,&session),"CreateSession");
        actions.initialize(api.GetInstanceProcAddr,instance,session);
        XrReferenceSpaceCreateInfo rc{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};rc.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;rc.poseInReferenceSpace.orientation.w=1;
        check(api.CreateReferenceSpace(session,&rc,&space),"CreateLocalSpace");
        rc.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_VIEW;
        check(api.CreateReferenceSpace(session,&rc,&head_space),"CreateHeadSpace");
        // build165 item 18: optional STAGE space = the runtime floor. Failure is not a fault: the EXE falls back to the countdown.
        rc.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_STAGE;
        if(XR_FAILED(api.GetInstanceProcAddr(instance,"xrLocateSpace",reinterpret_cast<PFN_xrVoidFunction*>(&locate_floor)))||!locate_floor||
           XR_FAILED(api.CreateReferenceSpace(session,&rc,&stage_space))){stage_space={};locate_floor=nullptr;log("STAGE_SPACE_UNAVAILABLE");}
        else log("STAGE_SPACE_READY");
        check(api.EnumerateSwapchainFormats(session,0,&n,nullptr),"FormatCount");demand(n&&n<=256,"XR_FORMAT_COUNT");
        std::vector<int64_t> formats(n);check(api.EnumerateSwapchainFormats(session,n,&n,formats.data()),"Formats");
        // Task123: display-encoded BGRA bytes, not HDR, go into the matching sRGB swapchain.
        for(auto f:{DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,DXGI_FORMAT_B8G8R8A8_UNORM})
            if(!selected_format&&std::find(formats.begin(),formats.end(),f)!=formats.end()) selected_format=f;
        // 167-neural A2: a runtime without BGRA swapchains gets RGBA ones; every copy into them becomes a draw (swap_copy)
        for(auto f:{DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,DXGI_FORMAT_R8G8B8A8_UNORM})
            if(!selected_format&&std::find(formats.begin(),formats.end(),f)!=formats.end()){selected_format=f;swapRgba=true;}
        demand(selected_format,"XR_SWAPCHAIN_FORMAT");
        for(unsigned e=0;e<2;++e) {
            demand(configs[e].maxImageRectWidth>=w&&configs[e].maxImageRectHeight>=h,"XR_IMAGE_SIZE");
            XrSwapchainCreateInfo sw{XR_TYPE_SWAPCHAIN_CREATE_INFO};sw.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
            sw.format=selected_format;sw.sampleCount=1;sw.width=w;sw.height=h;sw.faceCount=sw.arraySize=sw.mipCount=1;
            check(api.CreateSwapchain(session,&sw,&eyes[e].swap),"CreateSwapchain");
            check(api.EnumerateSwapchainImages(eyes[e].swap,0,&n,nullptr),"ImageCount");demand(n&&n<=16,"XR_IMAGE_COUNT");
            eyes[e].images.assign(n,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
            check(api.EnumerateSwapchainImages(eyes[e].swap,n,&n,reinterpret_cast<XrSwapchainImageBaseHeader*>(eyes[e].images.data())),"Images");
        }
        log("EYE_VIEWS recommended="+std::to_string(configs[0].recommendedImageRectWidth)+"x"+std::to_string(configs[0].recommendedImageRectHeight)+","+
            std::to_string(configs[1].recommendedImageRectWidth)+"x"+std::to_string(configs[1].recommendedImageRectHeight)+" max="+
            std::to_string(configs[0].maxImageRectWidth)+"x"+std::to_string(configs[0].maxImageRectHeight)+" chosen="+std::to_string(w)+"x"+std::to_string(h));
        LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);timingFrequency=frequency.QuadPart;
        XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};check(api.GetInstanceProperties(instance,&properties),"InstanceProperties");
        log("INITIALIZED width="+std::to_string(w)+" height="+std::to_string(h)+" format="+std::to_string(selected_format)+(swapRgba?" swap=rgba":"")+" qpc_frequency="+std::to_string(frequency.QuadPart)+" runtime="+properties.runtimeName+" runtime_version="+std::to_string(properties.runtimeVersion));
        health("initialize_after");
    }
    void poll() {
        for(unsigned i=0;i<64;++i) {
            XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};auto r=api.PollEvent(instance,&event);
            if(r==XR_EVENT_UNAVAILABLE)return;check(r,"PollEvent");
            if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING){terminal=true;return;}
            if(event.type==XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING){
                const auto&change=*reinterpret_cast<const XrEventDataReferenceSpaceChangePending*>(&event);
                if(change.session==session&&change.referenceSpaceType==XR_REFERENCE_SPACE_TYPE_LOCAL)pauseSpaceChange=change.changeTime;
                continue;
            }
            if(event.type!=XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)continue;
            auto&s=*reinterpret_cast<XrEventDataSessionStateChanged*>(&event);demand(s.session==session,"XR_EVENT_SESSION");state=s.state;
            log("STATE value="+std::to_string(state));
            if(state==XR_SESSION_STATE_READY||state==XR_SESSION_STATE_STOPPING)pauseAnchor.reset();
            if(state==XR_SESSION_STATE_READY&&!running){XrSessionBeginInfo b{XR_TYPE_SESSION_BEGIN_INFO};b.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;check(api.BeginSession(session,&b),"BeginSession");running=true;}
            if(state==XR_SESSION_STATE_STOPPING&&running){demand(!begun,"XR_STOP_WITH_OPEN_FRAME");check(api.EndSession(session),"EndSession");running=false;}
            if(state==XR_SESSION_STATE_EXITING||state==XR_SESSION_STATE_LOSS_PENDING)terminal=true;
        }
        throw std::runtime_error("XR_EVENT_BOUND");
    }
    bool restartable_terminal(const char*reason)const {
        for(const auto&p:uiPanels)if(p.eye.acquired)return false;
        for(const auto&hand:scopeEye)for(const auto&eye:hand)if(eye.acquired)return false;
        return reason&&!strcmp(reason,"XR_TERMINAL")&&terminal&&!fault&&!begun&&
            !eyes[0].acquired&&!eyes[1].acquired&&!scopePanelEye.acquired&&!overlayEye.acquired&&
            !nativeMenuPanel.eye.acquired&&!wristEye.acquired&&!weaponHudEye.acquired&&!rightUiEye.acquired&&!elementUiEye.acquired&&!wantedUiPanel.eye.acquired;
    }
    void pose_receipt(bool rejected,bool renormalized) {
        if(renormalized&&!poseRenormalizedLogged){poseRenormalizedLogged=true;log("XR_POSE_RENORMALIZED");}
        if(rejected&&(++poseRejected==1||poseRejected%120==0))log("XR_POSE_REJECTED");
    }
    GxFrame begin(uint64_t gen) {
        const long long perfStart147=Perf147::now();perf147.steps.reset(perfStart147);
        demand(!fault&&!begun&&gen,"XR_BEGIN_STATE");health("begin_before");perf147.cpu("poll_events",[&](){poll();});demand(!terminal,"XR_TERMINAL");
        GxFrame out{};out.size=sizeof(out);out.generation=gen;copied=0;visible=false;reset_frame_presentation();
        if(!running)return out;
        XrFrameWaitInfo wi{XR_TYPE_FRAME_WAIT_INFO};check(perf147.cpu("xrWaitFrame",[&](){return api.WaitFrame(session,&wi,&timing);}),"WaitFrame");
        {   // build133 FRAME_TIMING: delivered cadence between successive WaitFrame returns, every 600 frames.
            LARGE_INTEGER now;QueryPerformanceCounter(&now);
            if(timingLast&&timingFrequency>0){const double ms=double(now.QuadPart-timingLast)*1000.0/double(timingFrequency);
                const double period=double(timing.predictedDisplayPeriod)/1e6;++timingFrames;timingSumMs+=ms;if(ms>timingMaxMs)timingMaxMs=ms;
                if(period>0&&ms>period*1.5)++timingLate;
                if(timingFrames>=600){std::ostringstream s;s<<std::fixed<<std::setprecision(2)<<"FRAME_TIMING frames="<<timingFrames<<" avg_ms="<<timingSumMs/double(timingFrames)
                    <<" max_ms="<<timingMaxMs<<" period_ms="<<period<<" late="<<timingLate<<" width="<<width<<" height="<<height;log(s.str());
                    timingFrames=timingLate=0;timingSumMs=timingMaxMs=0;}}
            timingLast=now.QuadPart;
        }
        if(pauseSpaceChange&&timing.predictedDisplayTime>=pauseSpaceChange){pauseAnchor.reset();pauseSpaceChange=0;}
        XrFrameBeginInfo bi{XR_TYPE_FRAME_BEGIN_INFO};check(perf147.cpu("xrBeginFrame",[&](){return api.BeginFrame(session,&bi);}),"BeginFrame");begun=true;++frame;generation=gen;
        perf147.open(device.Get(),context.Get(),frame,generation);
        health("begin_after");
        XrViewLocateInfo li{XR_TYPE_VIEW_LOCATE_INFO};li.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;li.displayTime=timing.predictedDisplayTime;li.space=space;
        XrViewState vs{XR_TYPE_VIEW_STATE};uint32_t count=0;check(perf147.cpu("xrLocateViews",[&](){return api.LocateViews(session,&li,&vs,2,&count,rendered.data());}),"LocateViews");
        last_pose_tick=GetTickCount64();
        constexpr XrViewStateFlags valid=XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_TRACKED_BIT|XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
        visible=timing.shouldRender&&count==2&&(vs.viewStateFlags&valid)==valid&&state==XR_SESSION_STATE_FOCUSED;
        std::array<pose075::Pose,2> ps{pose(rendered[0].pose),pose(rendered[1].pose)};
        bool rejected=false,renormalized=false;
        for(unsigned e=0;e<count&&e<2;++e){
            const auto result=gx_pose::validate(ps[e]);
            rejected|=result==gx_pose::Rejected;renormalized|=result==gx_pose::Renormalized;
            if(result==gx_pose::Renormalized)rendered[e].pose.orientation={float(ps[e].qx),float(ps[e].qy),float(ps[e].qz),float(ps[e].qw)};
        }
        visible=visible&&!rejected;
        if(!anchored) {
            if(visible&&startup.accept(true,true,ps)){
                auto midpoint=shared086::midpoint(ps[0],ps[1]);const auto result=gx_pose::validate(midpoint);
                rejected|=result==gx_pose::Rejected;renormalized|=result==gx_pose::Renormalized;
                if(!rejected){reference=midpoint;anchored=true;log("REFERENCE_READY");}
                else{startup.stable=0;visible=false;}
            }
            else{if(!visible)startup.stable=0;visible=false;}
        }
        pose_receipt(rejected,renormalized);
        out.id=frame;out.display_time=timing.predictedDisplayTime;out.renderable=visible;out.reference=pod(reference);
        for(unsigned e=0;e<2;++e){out.eyes[e]=pod(ps[e]);auto&f=rendered[e].fov;out.fov[e]={f.angleLeft,f.angleRight,f.angleUp,f.angleDown};}
        if(visible&&!fovLogged){fovLogged=true;std::ostringstream s;s<<std::setprecision(6)<<"EYE_FOV left="<<rendered[0].fov.angleLeft<<','<<rendered[0].fov.angleRight<<','
            <<rendered[0].fov.angleUp<<','<<rendered[0].fov.angleDown<<" right="<<rendered[1].fov.angleLeft<<','<<rendered[1].fov.angleRight<<','<<rendered[1].fov.angleUp<<','
            <<rendered[1].fov.angleDown<<" width="<<width<<" height="<<height;log(s.str());}
        if(visible&&(submitted==0||frame%120==0)){
            std::ostringstream s;s<<std::setprecision(10)<<"POSE frame="<<frame<<" generation="<<generation<<" reference="<<reference.x<<','<<reference.y<<','<<reference.z<<','<<reference.qx<<','<<reference.qy<<','<<reference.qz<<','<<reference.qw;
            for(unsigned e=0;e<2;++e){auto&p=ps[e];auto&f=out.fov[e];s<<" eye"<<e<<'='<<p.x<<','<<p.y<<','<<p.z<<','<<p.qx<<','<<p.qy<<','<<p.qz<<','<<p.qw<<" fov"<<e<<'='<<f.left<<','<<f.right<<','<<f.up<<','<<f.down;}
            log(s.str());
        }
        perf147.cpuRecord(frame,generation,"begin",timing.predictedDisplayTime,timing.predictedDisplayPeriod,timing.shouldRender!=0,perfStart147,Perf147::now());
        return out;
    }
#include "frame_capture.inl"
    GxInput read_input() {
        demand(!fault,"XR_INPUT_FAULT");
        // Native input also runs when gameplay is paused. Do not depend on PrepareCamera.
        if(!begun)poll();
        demand(!terminal,"XR_TERMINAL");
        auto out=actions.read(running&&!terminal,state==XR_SESSION_STATE_FOCUSED);
        const uint64_t hapticNow=GetTickCount64();
        const bool freshRumble=rumbleTick&&hapticNow-rumbleTick<=200;
        spatialOwnerGate.input(hapticNow,freshRumble?nativeRumble.heavy:0,freshRumble?nativeRumble.light:0);
        // Only the original motor envelope is retained/renewed here. No old spatial packet.
        if(!actions.native_rumble(spatialOwnerGate.issuedHeavy,spatialOwnerGate.issuedLight))
            if(!compact_logging&&(frame<=5||frame%120==0))log("NATIVE_HAPTIC_UNAVAILABLE frame="+std::to_string(frame));
        if(!running||terminal||!anchored||state!=XR_SESSION_STATE_FOCUSED||!timing.predictedDisplayTime||GetTickCount64()-last_pose_tick>250){spatialOwnerGate.owners(out);return out;}
        XrViewLocateInfo li{XR_TYPE_VIEW_LOCATE_INFO};li.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        li.displayTime=timing.predictedDisplayTime+timing.predictedDisplayPeriod;li.space=space;
        std::array<XrView,2> v{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};
        XrViewState vs{XR_TYPE_VIEW_STATE};uint32_t count=0;
        check(api.LocateViews(session,&li,&vs,2,&count,v.data()),"InputLocateViews");
        constexpr XrViewStateFlags tracked=XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_TRACKED_BIT|XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
        if(count==2&&(vs.viewStateFlags&tracked)==tracked){
            std::array<pose075::Pose,2> ps{pose(v[0].pose),pose(v[1].pose)};
            const auto left=gx_pose::validate(ps[0]),right=gx_pose::validate(ps[1]);
            bool rejected=left==gx_pose::Rejected||right==gx_pose::Rejected;
            bool renormalized=left==gx_pose::Renormalized||right==gx_pose::Renormalized;
            if(!rejected){
                auto head=shared086::midpoint(ps[0],ps[1]);const auto result=gx_pose::validate(head);
                rejected=result==gx_pose::Rejected;renormalized|=result==gx_pose::Renormalized;
                if(!rejected){out.head=pod(head);out.reference=pod(reference);out.head_valid=1;out.display_time=li.displayTime;
                    panelHead=out.head;panelHeadValid=true;} // build167 item 1: face the head
            }
            pose_receipt(rejected,renormalized);
        }
        spatialOwnerGate.owners(out);
        return out;
    }
    // build165 item 18: height of the LOCAL origin above the STAGE floor (m); false = no floor this frame.
    bool read_floor(double&local_above_floor){
        if(!stage_space||!locate_floor||!running||terminal||!timing.predictedDisplayTime)return false;
        XrSpaceLocation l{XR_TYPE_SPACE_LOCATION};
        if(XR_FAILED(locate_floor(space,stage_space,timing.predictedDisplayTime,&l))||!(l.locationFlags&XR_SPACE_LOCATION_POSITION_VALID_BIT))return false;
        const double y=l.pose.position.y;
        if(!(y>-3.0&&y<3.0))return false;
        local_above_floor=y;return true;
    }
    void set_native_rumble(const GxRumble&v){
        demand(gx_haptic::valid(v),"RUMBLE_ABI");
        const uint64_t hapticNow=GetTickCount64();
        if(v.version==1){nativeRumble=v;rumbleTick=hapticNow;spatialOwnerGate.general();return;}
        // Final native owner commit only; never store or refresh the general clock with v2.
        if(!spatialOwnerGate.take(v,nativeRumble,hapticNow))return;
        if(!actions.spatial_rumble(gx_haptic::spatial(v,0),gx_haptic::spatial(v,1)))
            if(!compact_logging&&(frame<=5||frame%120==0))log("NATIVE_SPATIAL_HAPTIC_UNAVAILABLE frame="+std::to_string(frame));
    }
    void locate_hands(const GxFrame&f,const double*camera,GxHandTargets&out){
        demand(!fault&&begun&&f.size==sizeof(f)&&f.id==frame&&f.generation==generation&&
            f.display_time==timing.predictedDisplayTime&&camera&&out.size==sizeof(out)&&out.version==1,"HANDS_FRAME_ABI");
        out={};out.size=sizeof(out);out.version=1;out.frame=frame;out.generation=generation;out.display_time=f.display_time;
        if(!visible||!anchored||state!=XR_SESSION_STATE_FOCUSED)return;
        out.valid_mask=actions.locate(space,f.display_time,out.local);
        wristHandFrame=frame;wristHandValid=(out.valid_mask&1)!=0;wristGrip=out.local[0];
        hudHandFrame=frame;hudHandMask=out.valid_mask;hudGrip[0]=out.local[0];hudGrip[1]=out.local[1];
        hand_world(f,camera,out);
    }
    void locate_aim(const GxFrame&f,const double*camera,GxHandTargets&out){
        demand(!fault&&begun&&f.size==sizeof(f)&&f.id==frame&&f.generation==generation&&
            f.display_time==timing.predictedDisplayTime&&camera&&out.size==sizeof(out)&&out.version==1,"AIM_FRAME_ABI");
        out={};out.size=sizeof(out);out.version=1;out.frame=frame;out.generation=generation;out.display_time=f.display_time;
        if(!visible||!anchored||state!=XR_SESSION_STATE_FOCUSED)return;
        out.valid_mask=actions.locate(space,f.display_time,out.local,true);hand_world(f,camera,out);
    }
    gx_scope_source::Cache scopeSources;
    bool scopeSourceCaptureEnabled=false; // no extra allocations/copies by default
    bool capture_scope_source(const GxScopeSourceCapture&r,ID3D11Texture2D*source){
        if(!scopeSourceCaptureEnabled)return true;
        if(fault||!begun||!visible||panel||scopePanel.active||!GxScopeEyeFinalSource(r)||
           r.eye>=2||r.frame!=frame||r.generation!=generation||copied!=r.eye||r.stage!=1){scopeSources.invalidate();return false;}
        const bool copiedSource=perf147.gpu(context.Get(),"scope_source",int(r.eye),-1,[&](){return scopeSources.capture(context.Get(),source,r.eye,r.frame,r.generation,gx_scope_source::WorldBeforeUi,static_cast<DXGI_FORMAT>(selected_format));});
        health("scope_source_copy"); // actual device loss stays fatal, ordinary optional rejection does not
        return copiedSource;
    }
    unsigned right_element_layer_allowance(unsigned occupied)const {
        if(occupied>=uiMaxLayers)return 0;
        const unsigned available=uiMaxLayers-occupied;unsigned hands=0,modals=0;
        const auto head=pod(shared086::midpoint(pose(rendered[0].pose),pose(rendered[1].pose)));
        for(unsigned h=0;h<2;++h){const UiPixels*source=nullptr;bool conflict=false;
            for(const auto&p:uiPixels)if(p.texture&&p.captured==frame&&gx_native_ui::handRoute(p.item.route)&&
                gx_native_ui::routeHand(p.item.route)==h){if(source)conflict=true;source=&p;}
            if(!conflict&&source&&gx_native_ui::handItem(source->item)&&source->item.width<=uiMaxWidth&&source->item.height<=uiMaxHeight&&
               gx_hand_display::fresh(h,source->captured,hudHandFrame,frame,hudHandMask,hudGrip[h],head))++hands;
        }
        for(const auto&p:uiPixels)if(p.texture&&p.captured==frame&&gx_ui_visibility::product(uiVisibility,p.item)&&!gx_native_ui::handRoute(p.item.route)&&
            !gx_native_ui::wantedItem(p.item)&&gx_native_ui::fitRoute(p.item)==GX_UI_PANEL&&
            p.item.width<=uiMaxWidth&&p.item.height<=uiMaxHeight)++modals;
        modals=(std::min)(modals,unsigned(uiPanels.size()));
        // Reserve the same feasible hand/modal layers77 could show AFTER its
        // one RIGHT layer. Custom quads cannot silently crowd out those layers.
        const unsigned buttons=buttons_source()?1u:0u; // build167 item 3: the wrist buttons card keeps its layer
        const unsigned reserve=(std::min)(available-1,hands+modals+buttons);
        return available-reserve;
    }
#include "frame_finish.inl"
    void close() {
        perf147.close(context.Get());
        if(!api.module){reset_frame_presentation();return;}
        actions.stop_rumble();rumbleTick=0;close_scope_optics();scopeSources.close();scopeSourceCaptureEnabled=false;spatialOwnerGate=gx_haptic::ImmediateOwnerGate();
        release_frame_images();
        if(begun)finish(0,0,true);
        close_eye_chain(); // 167-neural B2: before the swapchains it draws into
        close_overlay();
        close_wrist();
        close_weapon_hud();
        close_hand_contexts();close_buttons_layer();close_fade_layer();close_native_ui();
        if(session&&running&&!terminal){check(api.RequestExitSession(session),"RequestExit");auto deadline=GetTickCount64()+2000;while(running&&GetTickCount64()<deadline){poll();if(running)Sleep(1);}demand(!running,"XR_EXIT_TIMEOUT");}
        destroy_scope_panel();reset_frame_presentation();
        destroy_panel();
        for(auto&e:eyes){e.images.clear();if(e.swap){check(api.DestroySwapchain(e.swap),"DestroySwapchain");e.swap={};}}
        if(stage_space){api.DestroySpace(stage_space);stage_space={};locate_floor=nullptr;} // build165 item 18: never a fault
        if(head_space){check(api.DestroySpace(head_space),"DestroyHeadSpace");head_space={};}
        actions.close_spaces(); // action spaces are session children, destroy BEFORE session.
        if(space){check(api.DestroySpace(space),"DestroySpace");space={};}if(session){check(api.DestroySession(session),"DestroySession");session={};}
        actions.close();
        if(instance){check(api.DestroyInstance(instance),"DestroyInstance");instance={};}context.Reset();device.Reset();api.Unload();log("CLOSED submitted="+std::to_string(submitted)+" menu_frames="+std::to_string(menu_frames));
    }
};
}
