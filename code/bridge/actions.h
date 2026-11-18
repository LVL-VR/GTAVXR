#pragma once
#include "interaction_profiles.h" // 167-neural A2
#include "input.h"
#include "haptics.h"
#include "haptic_publication.h"
#include "pose_validation.h"
#include "psvr2_input.h"
#include <windows.h>
#include <openxr/openxr.h>
#include <array>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>
#include <cmath>

namespace native_xr {
// GTAVXR-owned implementation. Routing/API semantics: pinned5VR XRActionManager and
// Khronos Touch profile. No imported game code or driver/OS input emulation.
class Actions {
    PFN_xrStringToPath stringPath{};
    PFN_xrCreateActionSet createSet{};
    PFN_xrDestroyActionSet destroySet{};
    PFN_xrCreateAction createAction{};
    PFN_xrSuggestInteractionProfileBindings suggest{};
    PFN_xrAttachSessionActionSets attach{};
    PFN_xrSyncActions sync{};
    PFN_xrGetActionStateBoolean boolean{};
    PFN_xrGetActionStateFloat scalar{};
    PFN_xrGetActionStateVector2f vector{};
    PFN_xrGetActionStatePose poseState{};
    PFN_xrCreateActionSpace createSpace{};
    PFN_xrLocateSpace locateSpace{};
    PFN_xrDestroySpace destroySpace{};
    PFN_xrApplyHapticFeedback applyHaptic{};
    PFN_xrStopHapticFeedback stopHaptic{};
    PFN_xrGetCurrentInteractionProfile currentProfile{};
    PFN_xrPathToString pathString{};
    PFN_xrEnumerateBoundSourcesForAction boundSources{};
    PFN_xrGetInputSourceLocalizedName sourceName{};
    XrInstance instance{};
    XrSession session{};
    XrActionSet set{};
    XrPath hands[2]{};
    XrAction stick{},trigger{},grip{},primary{},secondary{},click{},menu{},secondaryTouch{},dpadShift{};
    XrAction gripPose{},aimPose{};XrSpace gripSpaces[2]{},aimSpaces[2]{};bool synced=false;
    XrAction rumble{};bool rumbling[2]={false,false};
    XrAction eyeGaze{};XrSpace eyeGazeSpace{}; // XR_EXT_eye_gaze_interaction, only when the instance enabled it
    gx_haptic::HapticPublication hapticPublication;
    uint32_t activeMask=0;
public:
    gx_psvr2::Remap remap;       // PSVR2 input post-processing; inert when psvr2 is false.
    uint32_t unsupportedProfiles=0; // Bit p corresponds to kProfiles[p].
    bool psvr2=false;            // SteamVR systemName playstation_vr2; set before the first read.
    bool eyeGazeRequested=false; // instance has XR_EXT_eye_gaze_interaction and the system supports it; set before initialize
    bool eyeGazeBound=false;     // the eye gaze profile suggestion was accepted
    uint32_t physicalSeen=0,physicalNew=0; // GX_* bits from the runtime before PSVR2 synthesis; New = first seen this read.
    bool modifierSeen=false,modifierNew=false;
private:
    uint64_t sequence=0;
    static void ok(XrResult r,const char*where) {
        if(XR_FAILED(r))throw std::runtime_error(std::string("XR_ACTION_")+where+":"+std::to_string(r));
    }
    XrPath path(const char*p){XrPath out{};ok(stringPath(instance,p,&out),p);return out;}
    XrAction make(const char*name,XrActionType type) {
        XrActionCreateInfo c{XR_TYPE_ACTION_CREATE_INFO};
        strcpy_s(c.actionName,name);strcpy_s(c.localizedActionName,name);
        c.actionType=type;c.countSubactionPaths=2;c.subactionPaths=hands;
        XrAction out{};ok(createAction(set,&c,&out),name);return out;
    }
public:
    void initialize(PFN_xrGetInstanceProcAddr get,XrInstance i,XrSession s) {
        instance=i;session=s;
#define GX_LOAD(member,name) ok(get(instance,"xr" #name,reinterpret_cast<PFN_xrVoidFunction*>(&member)),#name); if(!member)throw std::runtime_error("XR_ACTION_NULL_" #name)
        GX_LOAD(stringPath,StringToPath);GX_LOAD(createSet,CreateActionSet);
        GX_LOAD(destroySet,DestroyActionSet);GX_LOAD(createAction,CreateAction);
        GX_LOAD(suggest,SuggestInteractionProfileBindings);GX_LOAD(attach,AttachSessionActionSets);
        GX_LOAD(sync,SyncActions);GX_LOAD(boolean,GetActionStateBoolean);
        GX_LOAD(scalar,GetActionStateFloat);GX_LOAD(vector,GetActionStateVector2f);
        GX_LOAD(poseState,GetActionStatePose);GX_LOAD(createSpace,CreateActionSpace);
        GX_LOAD(locateSpace,LocateSpace);GX_LOAD(destroySpace,DestroySpace);
        GX_LOAD(applyHaptic,ApplyHapticFeedback);GX_LOAD(stopHaptic,StopHapticFeedback);
#undef GX_LOAD
        // Diagnostics only: missing entry points leave the profile unreported.
        get(instance,"xrGetCurrentInteractionProfile",reinterpret_cast<PFN_xrVoidFunction*>(&currentProfile));
        get(instance,"xrPathToString",reinterpret_cast<PFN_xrVoidFunction*>(&pathString));
        get(instance,"xrEnumerateBoundSourcesForAction",reinterpret_cast<PFN_xrVoidFunction*>(&boundSources));
        get(instance,"xrGetInputSourceLocalizedName",reinterpret_cast<PFN_xrVoidFunction*>(&sourceName));
        hands[0]=path("/user/hand/left");hands[1]=path("/user/hand/right");
        XrActionSetCreateInfo c{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy_s(c.actionSetName,"gtavxr_controls");strcpy_s(c.localizedActionSetName,"GTAVXR Controls");
        ok(createSet(instance,&c,&set),"CreateSet");
        stick=make("stick",XR_ACTION_TYPE_VECTOR2F_INPUT);
        trigger=make("trigger",XR_ACTION_TYPE_FLOAT_INPUT);grip=make("grip",XR_ACTION_TYPE_FLOAT_INPUT);
        primary=make("primary",XR_ACTION_TYPE_BOOLEAN_INPUT);secondary=make("secondary",XR_ACTION_TYPE_BOOLEAN_INPUT);
        click=make("stick_click",XR_ACTION_TYPE_BOOLEAN_INPUT);menu=make("menu",XR_ACTION_TYPE_BOOLEAN_INPUT);
        secondaryTouch=make("secondary_touch",XR_ACTION_TYPE_BOOLEAN_INPUT);
        {XrActionCreateInfo d{XR_TYPE_ACTION_CREATE_INFO};strcpy_s(d.actionName,"dpad_shift");
         strcpy_s(d.localizedActionName,"D-pad Shift Thumbrest Touch");d.actionType=XR_ACTION_TYPE_BOOLEAN_INPUT;
         d.countSubactionPaths=2;d.subactionPaths=hands;ok(createAction(set,&d,&dpadShift),"dpad_shift");}
        gripPose=make("hand_pose",XR_ACTION_TYPE_POSE_INPUT);
        aimPose=make("aim_pose",XR_ACTION_TYPE_POSE_INPUT);
        rumble=make("native_rumble",XR_ACTION_TYPE_VIBRATION_OUTPUT);
        // Suggest each controller profile independently. The runtime may leave actions unbound
        // or assign its own bindings. Lack of a suggested profile does not prevent headset rendering.
        unsupportedProfiles=0;
        const XrAction slots[gx_profiles::SLOT_COUNT]={stick,trigger,grip,click,primary,secondary,menu,gripPose,aimPose,rumble,secondaryTouch,dpadShift};
        for(unsigned p=0;p<gx_profiles::kProfileCount;++p){
            const gx_profiles::Profile&profile=gx_profiles::kProfiles[p];std::vector<XrActionSuggestedBinding> bindings;
            for(unsigned h=0;h<2;++h){
                const std::string prefix=h?"/user/hand/right/":"/user/hand/left/";
                for(unsigned k=0;k<gx_profiles::SLOT_COUNT;++k)if(*profile.hand[h][k])bindings.push_back({slots[k],path((prefix+profile.hand[h][k]).c_str())});
            }
            XrInteractionProfileSuggestedBinding b{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            b.interactionProfile=path(profile.path);
            b.countSuggestedBindings=uint32_t(bindings.size());b.suggestedBindings=bindings.data();
            XrResult r=suggest(instance,&b);
            if(XR_FAILED(r)){ // a runtime that rejects the PSVR2 touch paths keeps the original profile
                std::vector<XrActionSuggestedBinding> base;
                for(const auto&x:bindings)if(x.action!=secondaryTouch&&x.action!=dpadShift)base.push_back(x);
                if(base.size()!=bindings.size()){b.countSuggestedBindings=uint32_t(base.size());b.suggestedBindings=base.data();r=suggest(instance,&b);}
            }
            if(r==XR_ERROR_PATH_UNSUPPORTED)unsupportedProfiles|=1u<<p;
            else ok(r,profile.path);
        }
        eyeGazeBound=false;
        if(eyeGazeRequested){ // no subaction path: /user/eyes_ext only
            XrActionCreateInfo g{XR_TYPE_ACTION_CREATE_INFO};strcpy_s(g.actionName,"eye_gaze");strcpy_s(g.localizedActionName,"Eye Gaze");
            g.actionType=XR_ACTION_TYPE_POSE_INPUT;ok(createAction(set,&g,&eyeGaze),"eye_gaze");
            XrActionSuggestedBinding gb{eyeGaze,path("/user/eyes_ext/input/gaze_ext/pose")};
            XrInteractionProfileSuggestedBinding b{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            b.interactionProfile=path("/interaction_profiles/ext/eye_gaze_interaction");b.countSuggestedBindings=1;b.suggestedBindings=&gb;
            eyeGazeBound=XR_SUCCEEDED(suggest(instance,&b));
        }
        XrSessionActionSetsAttachInfo a{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};a.countActionSets=1;a.actionSets=&set;
        ok(attach(session,&a),"Attach"); // exactly once, before BeginSession.
        for(unsigned h=0;h<2;++h){
            XrActionSpaceCreateInfo poseSpace{XR_TYPE_ACTION_SPACE_CREATE_INFO};poseSpace.action=gripPose;
            poseSpace.subactionPath=hands[h];poseSpace.poseInActionSpace.orientation.w=1;
            ok(createSpace(session,&poseSpace,&gripSpaces[h]),"CreateGripSpace");
            poseSpace.action=aimPose;ok(createSpace(session,&poseSpace,&aimSpaces[h]),"CreateAimSpace");
        }
        if(eyeGazeBound){XrActionSpaceCreateInfo gs{XR_TYPE_ACTION_SPACE_CREATE_INFO};gs.action=eyeGaze;gs.poseInActionSpace.orientation.w=1;
            if(XR_FAILED(createSpace(session,&gs,&eyeGazeSpace))){eyeGazeSpace={};eyeGazeBound=false;}}
    }
    GxInput read(bool running,bool focused) {
        GxInput out{};out.size=sizeof(out);out.version=1;out.sequence=++sequence;
        synced=false;
        activeMask=0;
        if(!running||!focused){stop_rumble();return out;}
        XrActiveActionSet active{set,XR_NULL_PATH};XrActionsSyncInfo s{XR_TYPE_ACTIONS_SYNC_INFO};
        s.countActiveActionSets=1;s.activeActionSets=&active;
        const auto r=sync(session,&s);if(r==XR_SESSION_NOT_FOCUSED){stop_rumble();return out;}ok(r,"Sync");
        synced=true;
        unsigned clickHands=0;bool leftTouch=false;
        for(unsigned h=0;h<2;++h){
            XrActionStateGetInfo g{XR_TYPE_ACTION_STATE_GET_INFO};g.subactionPath=hands[h];
            auto mark=[&](XrBool32 a){if(a)out.active_hands|=1u<<h;};
            auto f=[&](XrAction a){g.action=a;XrActionStateFloat v{XR_TYPE_ACTION_STATE_FLOAT};ok(scalar(session,&g,&v),"Float");mark(v.isActive);return v.isActive?v.currentState:0.f;};
            auto b=[&](XrAction a,uint32_t mask){g.action=a;XrActionStateBoolean v{XR_TYPE_ACTION_STATE_BOOLEAN};ok(boolean(session,&g,&v),"Boolean");mark(v.isActive);if(v.isActive&&v.currentState)out.buttons|=mask;return v.isActive!=0;};
            g.action=stick;XrActionStateVector2f v{XR_TYPE_ACTION_STATE_VECTOR2F};ok(vector(session,&g,&v),"Vector");mark(v.isActive);
            if(v.isActive){out.stick[h][0]=v.currentState.x;out.stick[h][1]=v.currentState.y;}
            out.trigger[h]=f(trigger);out.grip[h]=f(grip);
            b(primary,h?GX_A:GX_X);b(secondary,h?GX_B:GX_Y);
            if(b(click,h?GX_RCLICK:GX_LCLICK))clickHands|=1u<<h;
            b(menu,GX_MENU); // either hand: SteamVR users may bind right Options/Create too; unsuggested elsewhere
            if(!h&&psvr2)for(XrAction touch:{secondaryTouch,dpadShift}){g.action=touch;XrActionStateBoolean t{XR_TYPE_ACTION_STATE_BOOLEAN};
                ok(boolean(session,&g,&t),"Boolean");leftTouch|=t.isActive&&t.currentState;}
        }
        physicalNew=out.buttons&~physicalSeen;physicalSeen|=out.buttons;
        modifierNew=leftTouch&&!modifierSeen;modifierSeen|=leftTouch;
        remap.apply(out,psvr2,leftTouch,clickHands); // see psvr2_input.h
        activeMask=out.active_hands;return out;
    }
    bool native_rumble(float heavy,float light){
        const auto m=gx_haptic::touch(heavy,light);const float amplitude[2]={m.left,m.right};
        bool accepted=true;
        for(unsigned h=0;h<2;++h){
            const float value=synced&&(activeMask&(1u<<h))?amplitude[h]:0.f;
            if(!session||!rumble||!applyHaptic||!stopHaptic)continue;
            XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO};info.action=rumble;info.subactionPath=hands[h];
            if(value>0){
                XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};vibration.amplitude=value;
                vibration.duration=50000000;vibration.frequency=XR_FREQUENCY_UNSPECIFIED;
                const uint64_t issuedAt=GetTickCount64();
                const auto r=applyHaptic(session,&info,reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
                if(XR_FAILED(r)||r==XR_SESSION_NOT_FOCUSED){hapticPublication.failed(h);accepted=false;}
                else{hapticPublication.applied(h,value,issuedAt);rumbling[h]=true;}
            }else if(rumbling[h]){
                const auto r=stopHaptic(session,&info);
                if(XR_FAILED(r)||r==XR_SESSION_NOT_FOCUSED)hapticPublication.failed(h);else hapticPublication.stopped(h);
                if(XR_FAILED(r)&&r!=XR_ERROR_SESSION_NOT_RUNNING)accepted=false;
                rumbling[h]=false;
            }
        }
        return accepted;
    }
    // Spatial delivery does not replay or renew the game's cached general motors.
    bool spatial_rumble(float left,float right){
        const float amplitude[2]={gx_haptic::bound(left),gx_haptic::bound(right)};bool accepted=true;
        for(unsigned h=0;h<2;++h){
            if(amplitude[h]<=0)continue;
            if(!synced||!(activeMask&(1u<<h)))continue;
            if(!session||!rumble||!applyHaptic||!stopHaptic){accepted=false;continue;}
            const uint64_t issuedAt=GetTickCount64();
            if(!hapticPublication.known(h,issuedAt)){accepted=false;continue;}
            if(hapticPublication.covers(h,amplitude[h],issuedAt))continue;
            XrHapticActionInfo info={};info.type=XR_TYPE_HAPTIC_ACTION_INFO;info.action=rumble;info.subactionPath=hands[h];
            XrHapticVibration vibration={};vibration.type=XR_TYPE_HAPTIC_VIBRATION;vibration.amplitude=amplitude[h];
            vibration.duration=50000000;vibration.frequency=XR_FREQUENCY_UNSPECIFIED;
            const auto r=applyHaptic(session,&info,reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
            if(XR_FAILED(r)||r==XR_SESSION_NOT_FOCUSED){hapticPublication.failed(h);accepted=false;}
            else{hapticPublication.applied(h,amplitude[h],issuedAt);rumbling[h]=true;}
        }
        return accepted;
    }
    // "left=<profile> right=<profile>" after XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED.
    std::string current_profiles(){
        std::string out;
        for(unsigned h=0;h<2;++h){
            out+=h?" right=":"left=";
            XrInteractionProfileState st{XR_TYPE_INTERACTION_PROFILE_STATE};char text[XR_MAX_PATH_LENGTH]={};uint32_t n=0;
            if(session&&currentProfile&&pathString&&XR_SUCCEEDED(currentProfile(session,hands[h],&st))&&st.interactionProfile!=XR_NULL_PATH&&
               XR_SUCCEEDED(pathString(instance,st.interactionProfile,XR_MAX_PATH_LENGTH,&n,text)))out+=text;
            else out+="none";
        }
        return out;
    }
    // "menu=[Left Hand Create] dpad_shift=[]...": what the runtime bound each button action to, after a profile change.
    std::string bound_sources(){
        if(!session||!boundSources||!sourceName)return "unavailable";
        const std::pair<const char*,XrAction> named[]={{"menu",menu},{"primary",primary},{"secondary",secondary},{"stick_click",click},
            {"secondary_touch",secondaryTouch},{"dpad_shift",dpadShift}};
        std::string out;
        for(const auto&n:named){
            out+=std::string(out.empty()?"":" ")+n.first+"=[";
            XrBoundSourcesForActionEnumerateInfo e{XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};e.action=n.second;
            XrPath paths[8]{};uint32_t count=0;
            if(n.second&&XR_SUCCEEDED(boundSources(session,&e,8,&count,paths)))
                for(uint32_t k=0;k<count&&k<8;++k){
                    XrInputSourceLocalizedNameGetInfo g{XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO};g.sourcePath=paths[k];
                    g.whichComponents=XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT|XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;
                    char text[256]={};uint32_t len=0;
                    out+=std::string(k?",":"")+(XR_SUCCEEDED(sourceName(session,&g,sizeof(text),&len,text))?text:"?");
                }
            out+="]";
        }
        return out;
    }
    void stop_rumble(){synced=false;activeMask=0;native_rumble(0,0);}
    // Eye gaze pose in base at time. active = the runtime reports an eye tracker bound; valid = a usable orientation.
    bool locate_gaze(XrSpace base,XrTime time,XrPosef&out,bool&active){
        active=false;if(!synced||!eyeGazeSpace||!base||time<=0)return false;
        XrActionStateGetInfo g{XR_TYPE_ACTION_STATE_GET_INFO};g.action=eyeGaze;
        XrActionStatePose st{XR_TYPE_ACTION_STATE_POSE};if(XR_FAILED(poseState(session,&g,&st)))return false;
        active=st.isActive!=0;if(!active)return false;
        XrSpaceLocation l{XR_TYPE_SPACE_LOCATION};if(XR_FAILED(locateSpace(eyeGazeSpace,base,time,&l)))return false;
        if(!(l.locationFlags&XR_SPACE_LOCATION_ORIENTATION_VALID_BIT))return false;
        out=l.pose;return true;
    }
    uint32_t locate(XrSpace base,XrTime time,GxPose* out,bool aim=false) {
        out[0]=out[1]=GxPose{};
        if(!synced||!base||time<=0)return 0;
        uint32_t mask=0;
        constexpr XrSpaceLocationFlags required=XR_SPACE_LOCATION_POSITION_VALID_BIT|
            XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_TRACKED_BIT|
            XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
        for(unsigned h=0;h<2;++h){
            XrActionStateGetInfo g{XR_TYPE_ACTION_STATE_GET_INFO};g.action=aim?aimPose:gripPose;g.subactionPath=hands[h];
            XrActionStatePose active{XR_TYPE_ACTION_STATE_POSE};ok(poseState(session,&g,&active),"PoseState");
            const XrSpace s=aim?aimSpaces[h]:gripSpaces[h];
            if(!active.isActive||!s)continue;
            XrSpaceLocation l{XR_TYPE_SPACE_LOCATION};ok(locateSpace(s,base,time,&l),aim?"LocateAim":"LocateGrip");
            if((l.locationFlags&required)!=required)continue;
            const auto&p=l.pose;GxPose v{p.position.x,p.position.y,p.position.z,p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w};
            if(gx_pose::validate(v)==gx_pose::Rejected)continue;
            out[h]=v;mask|=1u<<h;
        }
        return mask;
    }
    void close_spaces(){stop_rumble();if(eyeGazeSpace){ok(destroySpace(eyeGazeSpace),"DestroyEyeGazeSpace");eyeGazeSpace={};}eyeGazeBound=false;for(auto&s:gripSpaces)if(s){ok(destroySpace(s),"DestroyGripSpace");s={};}
        for(auto&s:aimSpaces)if(s){ok(destroySpace(s),"DestroyAimSpace");s={};}}
    void close(){close_spaces();if(set){ok(destroySet(set),"DestroySet");set={};}session={};instance={};hapticPublication.reset();
        remap.reset();modifierSeen=modifierNew=false;physicalSeen=physicalNew=0;}
};
}
