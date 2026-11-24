#define NOMINMAX
#include <windows.h>
#include <openxr/openxr_loader_negotiation.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>
#include <array>
#include <d3d11.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <limits>

// Runs in the explicit OpenXR layer selected by the test launcher.
// The GTA profile script supplies a short heartbeat before grips can be consumed.
namespace {
std::mutex lock;
PFN_xrGetInstanceProcAddr next;
PFN_xrCreateAction createAction;
PFN_xrCreateActionSpace createSpace;
PFN_xrLocateViews locateViews;
PFN_xrLocateSpace locateSpace;
PFN_xrGetActionStateFloat readFloat;
PFN_xrGetActionStateBoolean readBoolean;
PFN_xrPathToString pathString;
XrInstance instance;
XrAction pose, grip;
XrAction secondary;
bool yDown=false, swallowY=false;
struct Hand { XrSpace space{}; XrPath path{}; int zone=-1,held=-1; bool high=true; ULONGLONG seen=0,press=0; XrVector3f position{}; };
Hand hands[2];
struct HipPress { int zone,age; float x,y,z; };
HipPress latestPress[2]{};
bool pressDirty[2]{},gripDown[2]{};
ULONGLONG lastTracked[2]{};
ULONGLONG heartbeat=0;
int event=0;
struct SlotBounds { float left,right,bottom,top,back,front; };
constexpr SlotBounds bounds[2]={{.07f,.55f,-1.f,-.41f,-.53f,.20f},{.055f,.36f,-.17f,.19f,-.035f,.30f}};
#include "placement.inl"

// Head-relative metres; negative Z is in front of the player.
int bodySlot(float x,float y,float z,const Layout& layout=centers){
 int best=-1;float nearest=std::numeric_limits<float>::max();
 for(int i=0;i<4;i++){auto p=layout[i];const auto& b=bounds[i/2];float dx=x-p.x,dy=y-p.y,dz=z-p.z;
 const float halfGrabExtent=i<2?.272f:.32f;
 if(fabsf(dx)>(b.right-b.left)*halfGrabExtent||fabsf(dy)>(b.top-b.bottom)*halfGrabExtent||fabsf(dz)>(b.front-b.back)*halfGrabExtent)continue;
 if(i>=2){auto home=defaultLayout()[i];float sx=dx+home.x,sy=dy+home.y,sz=dz+home.z;
 if(sx*sx+sy*sy+sz*sz>=.39f*.39f)continue;}
 float dist=dx*dx+dy*dy+dz*dz;if(dist<nearest){nearest=dist;best=i;}}
 return best;
}
bool inventoryMode=false,inventoryOpen=false;
bool selectingCard=false;
ULONGLONG selectionFinishUntil=0;
int pendingHip[2]={-1,-1};
int ownedCards=0;
bool shoulderConsumed[2]{};
#include "category-wheel.inl"
int inventoryPage=0;
int touchedCard(XrVector3f hand){
 auto items=pageCards();
 if(!editor){
    float y=hand.y-wheelY,r=hand.x*hand.x+y*y;
    if(fabsf(hand.z-wheelZ)>=.09f||r<innerRadius*innerRadius||r>outerRadius*outerRadius)return -1;
    int sector=angleSector(hand.x,y);return items[sector]>=0?sector:-1;
 }
 for(size_t i=0;i<cards.size();i++)if(items[i]>=0 && fabsf(hand.x-cards[i].x)<.087f &&
    fabsf(hand.y-cards[i].y)<.042f && fabsf(hand.z-cards[i].z)<.09f)return i;
 return -1;
}
#include "four-editor.inl"
#include "markers.inl"

// Open the grabbing hand's wheel, then steer only while the HUD reports it open.
// Let the game see the shoulder pose before pressing grip, regardless of poll order.
bool pairedHips=false;
bool walkingTest=false;
bool nativeHipTap=false;
ULONGLONG neutralUntil[2]{};
XrAction walkTrigger{};
bool offhandCapture=false,offhandArmed=false,offhandPressed=false;
XrAction offhandPrimary{};
bool reloadReady=false,reloadPressed=false;
unsigned stowMask=0,triggerRelease=0,walkTaps=0;
int recallHand=-1,queuedHand=-1;
int hipMask=0,recallSlot=-1,queuedSlot=-1,sector=0,matches=0;
bool steering=false,confirmed=false;
ULONGLONG sectorSince=0,cancelling=0;
XrAction wheelStick{};
PFN_xrGetActionStateVector2f readVector{};
ULONGLONG recallStart=0,poseReady=0,gripStarted=0;
std::array<int,weaponCount> lastConfirmedSector{};
int sectorsTried=0;
void startRecall(int hand,ULONGLONG now){
 recallHand=hand;recallStart=now;poseReady=gripStarted=0;
 sector=(inventoryMode && recallSlot>=0 && recallSlot<weaponCount)?lastConfirmedSector[recallSlot]:0;
 sectorsTried=matches=0;steering=confirmed=false;sectorSince=cancelling=0;
}
XrResult XRAPI_CALL wheelVector(XrSession session,const XrActionStateGetInfo* info,XrActionStateVector2f* out){
 auto r=readVector(session,info,out);if(XR_FAILED(r))return r;
 std::lock_guard guard(lock);
 auto now=GetTickCount64();
 if(wheelStick&&info->action==wheelStick&&hands[1].path&&info->subactionPath==hands[1].path){
    static bool tilted=false;
    auto v=out->currentState;
    bool down=inventoryMode&&recallHand<0&&out->isActive&&heartbeat&&now-heartbeat<=250&&
              v.y<=(tilted?-.55f:-.75f)&&fabsf(v.x)<(tilted?.55f:.40f);
    rightStickHold(down,tilted,now);tilted=down;
    if(down&&stickTarget>=0){out->currentState={};return r;}
 }
 if(nativeHipTap && info->action==wheelStick && heartbeat && now-heartbeat<=250){
    for(int i=0;i<2;i++)if(info->subactionPath==hands[i].path && now<neutralUntil[i]){
       out->currentState={};return r;
    }
 }
 if(recallHand>=0 && info->action==wheelStick && info->subactionPath==hands[recallHand].path &&
    (gripStarted||nativeHipTap) && out->isActive && heartbeat && now-heartbeat<=250){
    float angle=sector*.7853981634f;
    out->currentState=steering?XrVector2f{sinf(angle),cosf(angle)}:XrVector2f{};
 }
 return r;
}
XrResult XRAPI_CALL handLocation(XrSpace space,XrSpace base,XrTime time,XrSpaceLocation* out){
 auto r=locateSpace(space,base,time,out);if(XR_FAILED(r))return r;
 std::lock_guard guard(lock);auto now=GetTickCount64();
 if(recallHand>=0 && (pairedHips||nativeHipTap) && heartbeat && now-heartbeat<=250 && now-recallStart<220 &&
    space==hands[recallHand].space && base==markerFrame.space && now-hands[recallHand].seen<100 &&
    (out->locationFlags&XR_SPACE_LOCATION_POSITION_TRACKED_BIT)){
    out->pose.position=markerAt(markerFrame,{recallHand?.20f:-.20f,0,.10f}).position;
    if(!poseReady)poseReady=now;
 }
 return r;
}
XrResult XRAPI_CALL action(XrActionSet set,const XrActionCreateInfo* info,XrAction* out){
 auto r=createAction(set,info,out);
 if(XR_SUCCEEDED(r)){std::lock_guard guard(lock);
 if(info->actionType==XR_ACTION_TYPE_VIBRATION_OUTPUT && !strcmp(info->actionName,"native_rumble"))rumble=*out;
 if(!strcmp(info->actionName,"stick_click"))stickClick=*out;
 if(!strcmp(info->actionName,"hand_pose"))pose=*out;
 if(!strcmp(info->actionName,"grip"))grip=*out;
 if(!strcmp(info->actionName,"trigger"))walkTrigger=*out;
 if(!strcmp(info->actionName,"primary"))offhandPrimary=*out;
 if(!strcmp(info->actionName,"stick"))wheelStick=*out;
 if(!strcmp(info->actionName,"secondary"))secondary=*out;}return r;
}
XrResult XRAPI_CALL space(XrSession session,const XrActionSpaceCreateInfo* info,XrSpace* out){
 auto r=createSpace(session,info,out);if(XR_FAILED(r))return r;
 std::lock_guard guard(lock);if(info->action!=pose)return r;
 char path[XR_MAX_PATH_LENGTH]{};uint32_t size=0;
 if(XR_SUCCEEDED(pathString(instance,info->subactionPath,sizeof(path),&size,path))){
 for(int i=0;i<2;i++)if(!strcmp(path,i?"/user/hand/right":"/user/hand/left")){
 hands[i]=Hand{};hands[i].space=*out;hands[i].path=info->subactionPath;}}
 return r;
}
XrResult XRAPI_CALL views(XrSession session,const XrViewLocateInfo* info,XrViewState* state,uint32_t cap,uint32_t* count,XrView* eyes){
 auto r=locateViews(session,info,state,cap,count,eyes);
 if(XR_FAILED(r)||cap<2||*count!=2||!eyes)return r;
 std::lock_guard guard(lock);auto now=GetTickCount64();if(!heartbeat||now-heartbeat>250)return r;
 tickConfirmations(session,now);
 if((state->viewStateFlags&3)!=3){for(auto& h:hands)h.zone=-1;return r;}
 auto p=eyes[0].pose.position,qpos=eyes[1].pose.position;auto q=eyes[0].pose.orientation;
 float yaw=atan2f(2*(q.w*q.y+q.x*q.z),1-2*(q.x*q.x+q.y*q.y)),c=cosf(yaw),s=sinf(yaw);
 markerFrame={session,info->space,info->displayTime,{(p.x+qpos.x)*.5f,(p.y+qpos.y)*.5f,(p.z+qpos.z)*.5f},c,s};
 for(int i=0;i<2;i++){auto& h=hands[i];if(!h.space)continue;
 XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
 if(XR_FAILED(locateSpace(h.space,info->space,info->displayTime,&loc)))continue;
 if(!(loc.locationFlags&XR_SPACE_LOCATION_POSITION_VALID_BIT))continue;
 if(loc.locationFlags&XR_SPACE_LOCATION_POSITION_TRACKED_BIT)lastTracked[i]=now;
 else if(!lastTracked[i] || now-lastTracked[i]>350)continue;
 auto v=loc.pose.position;float dx=v.x-(p.x+qpos.x)*.5f,dy=v.y-(p.y+qpos.y)*.5f,dz=v.z-(p.z+qpos.z)*.5f;
 float x=c*dx-s*dz,z=s*dx+c*dz;
 h.position={x,dy,z};
 int zone=bodySlot(x,dy,z);
 if(editor)zone=bodySlot(x,dy,z,draftLayout);
 if(zone>=0&&zone!=h.zone)enteredZone[i]=now;
 pulseSlot(session,i,zone,now);
 h.zone=zone;
 h.seen=now;}return r;
}
XrResult XRAPI_CALL button(XrSession session,const XrActionStateGetInfo* info,XrActionStateBoolean* out){
 auto r=readBoolean(session,info,out);if(XR_FAILED(r))return r;
 std::lock_guard guard(lock);
 if(stickClick && info->action==stickClick){
    for(int i=0;i<2;i++)if(hands[i].path&&hands[i].path==info->subactionPath){
       bool down=out->isActive&&out->currentState;
       auto now=GetTickCount64();

       if(inventoryOpen && heartbeat && now-heartbeat<=250 && (editor?editorHand<0||editorHand==i:menuHand==i)){
          if(down&&!clickDown[i]&&now-hands[i].seen<175){
             int category=touchedCard(hands[i].position);
             if(category>=0&&category<8){cycleCategory(category);swallowClick[i]=true;}
          }
       }
       clickDown[i]=down;
       if(swallowClick[i]){out->currentState=XR_FALSE;out->changedSinceLastSync=XR_FALSE;if(!down)swallowClick[i]=false;}
    }
    return r;
 }
 if(offhandCapture && offhandPrimary && info->action==offhandPrimary &&
    hands[0].path && info->subactionPath==hands[0].path && heartbeat && GetTickCount64()-heartbeat<=250){
    if(!out->isActive || inventoryOpen || selectingCard)reloadReady=false;
    else if(!out->currentState)reloadReady=true;
    else if(reloadReady){reloadPressed=true;reloadReady=false;}
    out->currentState=XR_FALSE;out->changedSinceLastSync=XR_FALSE;return r;
 }
 if(!secondary||info->action!=secondary||!hands[0].path||info->subactionPath!=hands[0].path)return r;
 bool down=out->isActive&&out->currentState;
 if(cancelling && recallHand>=0 && out->isActive){out->currentState=XR_TRUE;return r;}
 if(fitting&&down&&!yDown&&heartbeat&&GetTickCount64()-heartbeat<=250){
 fitting=false;editSlot=dragHand=-1;chordSince=0;waitRelease=true;
 for(auto& h:hands){h.held=-1;h.high=true;}
 event=10;swallowY=true;
 }
 yDown=down;
 if(swallowY){out->currentState=XR_FALSE;out->changedSinceLastSync=XR_FALSE;if(!down)swallowY=false;}
 return r;
}
XrResult XRAPI_CALL value(XrSession session,const XrActionStateGetInfo* info,XrActionStateFloat* out){
 auto r=readFloat(session,info,out);if(XR_FAILED(r))return r;
 std::lock_guard guard(lock);auto now=GetTickCount64();
 if(walkTrigger && info->action==walkTrigger){
    if(walkingTest && heartbeat && now-heartbeat<=250){
       for(int i=0;i<2;i++)if(hands[i].path && hands[i].path==info->subactionPath){
          unsigned bit=1u<<i;
          if(i==0 && offhandCapture){
             if(!out->isActive || inventoryOpen || selectingCard)offhandArmed=false;
             else if(out->currentState<.20f)offhandArmed=true;
             else if(out->currentState>=.70f && offhandArmed){offhandPressed=true;offhandArmed=false;}
             if(out->currentState>=.1f)triggerRelease|=bit;else triggerRelease&=~bit;
             out->currentState=0;out->changedSinceLastSync=XR_FALSE;continue;
          }
          if((stowMask|triggerRelease)&bit){
             if(out->currentState<.1f)triggerRelease&=~bit;
             else triggerRelease|=bit;
             out->currentState=0;out->changedSinceLastSync=XR_FALSE;
          }
       }
    }
    return r;
 }
 if(info->action!=grip)return r;
 for(auto& h:hands){if(!h.path||info->subactionPath!=h.path)continue;
 int idx=int(&h-hands);
 if(out->isActive && out->currentState>=.7f && !gripDown[idx]){
    latestPress[idx]={h.zone,h.seen?int(std::min<ULONGLONG>(60000,now-h.seen)):-1,h.position.x,h.position.y,h.position.z};
    pressDirty[idx]=gripDown[idx]=true;
 }else if(!out->isActive || out->currentState<.25f)gripDown[idx]=false;
 if(!out->isActive||!heartbeat||now-heartbeat>250){h.held=-1;h.high=true;continue;}
 float raw=out->currentState;
 int handIndex=int(&h-hands);squeezes[handIndex]=raw;gripSeen[handIndex]=now;
 if(inventoryMode && recallHand<0){
    bool wheelPress=raw>=.7f&&!h.high&&(!h.seen||now-h.seen>=175||nearOriginalWheel(handIndex,h.position));
    bool consumed=fourGrip(handIndex,raw,now);
    if(wheelPress||consumed)blockWheelGrip[handIndex]=true;
    if(blockWheelGrip[handIndex]){out->currentState=0;out->changedSinceLastSync=XR_FALSE;}
    if(raw<.25f)blockWheelGrip[handIndex]=false;
    continue;
 }
 if(inventoryMode && shoulderConsumed[handIndex]){
    if(raw<.25f){shoulderConsumed[handIndex]=false;h.high=false;}
    out->currentState=0;out->changedSinceLastSync=XR_FALSE;continue;
 }
 bool inventoryEquip=inventoryMode && recallHand>=0 && recallSlot>=0;
 if(nativeHipTap && !inventoryEquip && recallHand==handIndex){
    bool expired=now-recallStart>=220;
    if(expired || (gripStarted && now-gripStarted>=65)){
       out->currentState=0;out->changedSinceLastSync=gripStarted?XR_TRUE:XR_FALSE;
       selectingCard=false; if(gripStarted){neutralUntil[handIndex]=now+35;walkTaps|=1u<<handIndex;}
       startRecall(queuedHand,now);queuedHand=-1;
    }else if(poseReady && now-poseReady>=16){
       out->changedSinceLastSync=gripStarted?XR_FALSE:XR_TRUE;
       if(!gripStarted)gripStarted=now;
       out->currentState=1;
    }else{out->currentState=0;out->changedSinceLastSync=XR_FALSE;}
    h.high=raw>=.25f;continue;
 }
 if(inventoryMode && recallHand<0){
    bool fresh=h.seen && now-h.seen<
        175;
    if(raw>=.7f && !h.high && fresh && h.zone>=2 && h.zone<4){h.held=h.zone;h.high=true;h.press=now;}
    if(h.held>=2 && h.held<=3){
       if(raw>=.7f && fresh && h.zone==h.held && h.zone==2+handIndex && now-h.press>=350){
          if(!ownedCards){event=61;inventoryOpen=false;}else inventoryOpen=!inventoryOpen;
          inventoryPage=0;
          h.held=-1;shoulderConsumed[handIndex]=true;
       }
       if(raw<.25f){
          if(!inventoryOpen){
             bool atSlot=fresh && h.zone==h.held;
             if(now-h.press<350)pendingHip[handIndex]=h.held;
             else if(atSlot && h.held!=2+handIndex && now-h.press>=1000)pendingHip[handIndex]=8+h.held;
          }
          h.held=-1;h.high=false;
       }
       out->currentState=0;out->changedSinceLastSync=XR_FALSE;continue;
    }
    if(inventoryOpen){
       if(raw>=.7f && !h.high && fresh){
          int card=touchedCard(h.position);
          if(card>=0){h.held=4+pageCards()[card];h.press=now;}
       }
       if(raw<.25f && h.held>=4){
          int item=h.held-4;h.held=-1;int target=touchedCard(h.position);
          if(fresh && target>=0 && pageCards()[target]==item){
             if(item==nextPageCard)++inventoryPage;
             else{inventoryOpen=false;selectingCard=true;event=200+handIndex*weaponCount+item;
                  selectionFinishUntil=now+180;}
          }
       }
       h.high=raw>=.25f;out->currentState=0;out->changedSinceLastSync=XR_FALSE;continue;
    }
 }
 if(walkingTest && (!inventoryEquip || recallHand!=handIndex)){
    if(raw>=.7f&&!h.high){
       h.high=true;
       if(h.zone>=0 && h.zone<2 && h.seen && now-h.seen<
           175
           ){h.held=h.zone;h.press=now;}
    }
    if(h.held>=0){
       out->currentState=0;out->changedSinceLastSync=XR_FALSE;
       if(raw<.25f){
          if(inventoryMode && h.zone==h.held && now-h.seen<100 && now-h.press>=1000){
             pendingHip[handIndex]=8+h.held;
          }else if(
             now-h.press<650){
             if(inventoryMode)pendingHip[handIndex]=h.held;
             else if(!nativeHipTap)walkTaps|=1u<<handIndex;
             else if(recallHand<0){recallSlot=-1;startRecall(handIndex,now);}
             else queuedHand=handIndex;
          }
          h.held=-1;
       }
    }
    if(raw<.25f)h.high=false;
    continue;
 }
 if(recallHand==handIndex){
    if(!confirmed && !cancelling && (now-recallStart>=1750 || raw>=.7f)){
       cancelling=now;steering=false;
    }
    if(confirmed || (cancelling && now-cancelling>=80)){
       if(cancelling)event=inventoryMode?60:32;
       out->currentState=0;out->changedSinceLastSync=gripStarted?XR_TRUE:XR_FALSE;
       if(selectingCard && confirmed && !cancelling)selectionFinishUntil=now+180;
       selectingCard=false;recallSlot=queuedSlot;startRecall(queuedHand,now);queuedHand=queuedSlot=-1;
    }else if(poseReady && now-poseReady>=16){
       out->changedSinceLastSync=gripStarted?XR_FALSE:XR_TRUE;
       if(!gripStarted)gripStarted=now;
       out->currentState=1;
    }else{out->currentState=0;out->changedSinceLastSync=XR_FALSE;}
    h.high=raw>=.25f;continue;
 }
 if(waitRelease){
 if(squeezes[0]<.25f&&squeezes[1]<.25f){waitRelease=false;hands[0].high=hands[1].high=false;}
 out->currentState=0;out->changedSinceLastSync=XR_FALSE;continue;}
 if(fittingChord(now)){
 if(!chordSince)chordSince=now;
 if(now-chordSince>=1500){fitting=!fitting;waitRelease=true;chordSince=0;editSlot=dragHand=-1;
 for(auto& hand:hands){hand.held=-1;hand.high=true;}event=fitting?9:10;}
 out->currentState=0;out->changedSinceLastSync=XR_FALSE;continue;
 }else chordSince=0;
 if(fitting){
 if(handsInTogglePosition(now)){
 editSlot=dragHand=-1;h.high=raw>=.25f;h.held=-1;
 out->currentState=0;out->changedSinceLastSync=XR_FALSE;continue;}
 if(raw>=.7f&&!h.high){h.high=true;
 if(dragHand<0&&now-h.seen<100){editSlot=nearestSlot(h.position);dragHand=handIndex;event=20+editSlot;}}
 if(raw<.25f){h.high=false;if(dragHand==handIndex){
 event=(now-h.seen<100&&commitCenter(editSlot,h.position))?12+editSlot:16;
 editSlot=dragHand=-1;}}
 h.held=-1;out->currentState=0;out->changedSinceLastSync=XR_FALSE;continue;}
 if(editSlot>=0){
 bool rising=raw>=.7f&&!h.high;int index=int(&h-hands);
 if(rising){h.high=true;if(dragHand<0&&h.seen&&now-h.seen<100)dragHand=index;}
 if(raw<.25f){h.high=false;if(dragHand==index){
 if(now-h.seen<100&&commitCenter(editSlot,h.position))editSlot=-1;
 dragHand=-1;}}
 h.held=-1;out->currentState=0;out->changedSinceLastSync=XR_FALSE;continue;}
 if(raw>=.7f&&!h.high){h.high=true;if(h.zone>=0&&now-h.seen<100){h.held=h.zone;h.press=now;}}
 // Forward a sustained same-side shoulder grip once. GTAVXR owns its release.
 if(h.held==2+handIndex && now-h.press>=350 && raw>=.7f &&
    h.zone==h.held && now-h.seen<100){
    h.held=-1;out->changedSinceLastSync=XR_TRUE;continue;
 }
 if(h.held>=0){out->currentState=0;out->changedSinceLastSync=XR_FALSE;
 if(raw<.25f){if(h.zone==h.held&&now-h.seen<220 && (h.held!=2+handIndex || now-h.press<350)){
    if(h.held<2 && (hipMask&(1<<h.held)) && now-h.press<650){
       if(recallHand<0){recallSlot=h.held;startRecall(handIndex,now);}
       else{queuedHand=handIndex;queuedSlot=h.held;}
    }else event=1+h.held+(now-h.press>=650?4:0);
 }h.held=-1;}}
 if(raw<.25f)h.high=false;}return r;
}
XrResult XRAPI_CALL proc(XrInstance i,const char* name,PFN_xrVoidFunction* out){
 auto r=next(i,name,out);if(XR_FAILED(r))return r;
#define WRAP(n,fn) if(!strcmp(name,n))*out=reinterpret_cast<PFN_xrVoidFunction>(fn)
 WRAP("xrCreateAction",action);WRAP("xrCreateActionSpace",space);WRAP("xrLocateViews",views);WRAP("xrGetActionStateFloat",value);
 if(applyPulse)WRAP("xrApplyHapticFeedback",gamePulse);
 if(stopPulse)WRAP("xrStopHapticFeedback",gameStop);
 WRAP("xrGetActionStateBoolean",button);
 WRAP("xrLocateSpace",handLocation);
 WRAP("xrGetActionStateVector2f",wheelVector);
 WRAP("xrCreateSession",markerCreate);WRAP("xrEndFrame",markerEnd);WRAP("xrDestroySession",markerDestroy);return r;
}
XrResult XRAPI_CALL start(const XrInstanceCreateInfo* info,const XrApiLayerCreateInfo* layer,XrInstance* out){
 auto n=layer->nextInfo;auto copy=*layer;copy.nextInfo=n->next;auto r=n->nextCreateApiLayerInstance(info,&copy,out);
 if(XR_FAILED(r))return r;next=n->nextGetInstanceProcAddr;instance=*out;
#define GET(name,var) if(XR_FAILED(next(instance,name,reinterpret_cast<PFN_xrVoidFunction*>(&var))))return XR_ERROR_INITIALIZATION_FAILED
 GET("xrCreateAction",createAction);GET("xrCreateActionSpace",createSpace);GET("xrLocateViews",locateViews);GET("xrLocateSpace",locateSpace);GET("xrGetActionStateFloat",readFloat);GET("xrPathToString",pathString);
 GET("xrCreateSession",realCreateSession);GET("xrEndFrame",endFrame);GET("xrDestroySession",destroySession);
 GET("xrGetActionStateBoolean",readBoolean);
 GET("xrGetActionStateVector2f",readVector);
 GET("xrCreateSwapchain",createSwap);GET("xrDestroySwapchain",destroySwap);GET("xrEnumerateSwapchainImages",images);
 GET("xrAcquireSwapchainImage",acquire);GET("xrWaitSwapchainImage",waitImage);GET("xrReleaseSwapchainImage",releaseImage);
 GET("xrGetSystemProperties",systemProps);
 next(instance,"xrStopHapticFeedback",reinterpret_cast<PFN_xrVoidFunction*>(&stopPulse));confirmationReport={};
 next(instance,"xrApplyHapticFeedback",reinterpret_cast<PFN_xrVoidFunction*>(&applyPulse));rumble={};confirmations={};seeded=false;closeMenu();
 {std::lock_guard guard(lock);pose={};grip={};secondary={};yDown=swallowY=false;hands[0]=Hand{};hands[1]=Hand{};event=0;heartbeat=0;pairedHips=false;recallHand=queuedHand=-1;loadPlacement();}return r;
}
}
extern "C" __declspec(dllexport) int TakeHipPress(int hand,HipPress* press){
 std::lock_guard guard(lock);
 if(!press || hand<0 || hand>1 || !pressDirty[hand])return 0;
 *press=latestPress[hand];pressDirty[hand]=false;return 1;
}
extern "C" __declspec(dllexport) unsigned CatalogHash(int index){
 return index>=0&&index<weaponCount?catalog[index].hash:0;
}
extern "C" __declspec(dllexport) void VisibleItems(int* items){
 std::lock_guard guard(lock);auto visible=pageCards();std::copy_n(visible.begin(),8,items);
}
extern "C" __declspec(dllexport) void SetInventory(const int* flags){
 std::lock_guard guard(lock);std::copy_n(flags,weaponCount,available.begin());
}
extern "C" __declspec(dllexport) void SkipEmptyAssignmentHand(int target){
 std::lock_guard guard(lock);
 if(target<0||target>=8||stickTarget!=target||holdFinished)return;
 emptyAssignmentHands|=1<<(target/4);stickTarget=-1;stickSince=missingSince=0;
}
extern "C" __declspec(dllexport) int SaveSlotForHand(int hand,int slot,int item){
 std::lock_guard guard(lock);
 if(hand<0||hand>1||!inventoryMode||inventoryOpen||slot<0||slot>=4||item<0||item>=weaponCount||!available[item]||catalog[item].hash==0xA2719263u)return 0;
 draftLayout=centers;draftAssigned=assigned;draftAssigned[slot]=item;
 if(!saveEditor())return 0;
 confirmations[hand]={2,GetTickCount64()};return 1;
}
extern "C" __declspec(dllexport) void SyncSlots(int* items){
 if(!items)return;
 std::lock_guard guard(lock);if(!seeded)seedSlots(items);std::copy(assigned.begin(),assigned.end(),items);
}
extern "C" __declspec(dllexport) void ReportCardAmmo(int item,int clip,int total){
 if(item<0 || item>=weaponCount)return;
 std::lock_guard guard(lock);
 auto& current=cardAmmo[item];
 if(current.clip==clip && current.total==total)return;
 current={clip,total};

}
extern "C" __declspec(dllexport) int PollHolster(int eligible){
 std::lock_guard guard(lock);heartbeat=eligible?GetTickCount64():0;
 walkingTest=(eligible&8)!=0;
 inventoryMode=(eligible&128)!=0;
 if(!inventoryMode){blockWheelGrip[0]=blockWheelGrip[1]=false;confirmations={};clearStickHold();if(inventoryOpen)closeMenu();}
 if(!inventoryMode){inventoryOpen=selectingCard=false;selectionFinishUntil=0;pendingHip[0]=pendingHip[1]=-1;shoulderConsumed[0]=shoulderConsumed[1]=false;}
 nativeHipTap=walkingTest && (eligible&64)!=0;
 if(!walkingTest){stowMask=triggerRelease=walkTaps=0;}
 if(walkingTest && walkTaps){int i=(walkTaps&1)?0:1;walkTaps&=~(1u<<i);return 40+i;}
 hipMask=(eligible>>1)&3;pairedHips=hipMask!=0;
 if(!pairedHips&&!nativeHipTap){recallHand=queuedHand=-1;}
 static unsigned previous=0;unsigned keys=0;
 if(markersEnabled&&!inventoryMode&&eligible&&(GetAsyncKeyState(VK_CONTROL)&0x8000)&&(GetAsyncKeyState(VK_SHIFT)&0x8000)){
 DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);
 if(pid==GetCurrentProcessId())for(int k=0;k<=4;k++)if(GetAsyncKeyState('0'+k)&0x8000)keys|=1u<<k;}
 for(int k=0;k<=4;k++)if((keys&~previous)&(1u<<k)){
 editSlot=k-1;dragHand=-1;event=0;for(auto& h:hands){h.held=-1;h.high=true;}}
 previous=keys;if(!eligible){if(fitting||waitRelease)resetFitting();dragHand=-1;chordSince=0;for(auto& h:hands){h.held=-1;h.high=true;}}
 int e=event;event=0;
 if(inventoryMode && recallHand<0 && !inventoryOpen && !e){
    for(int i=0;i<2;i++)if(pendingHip[i]>=0){int slot=pendingHip[i];pendingHip[i]=-1;return 70+4*i+slot;}
 }
 if((pairedHips||inventoryMode)&&!e&&recallHand>=0&&recallSlot>=0&&gripStarted&&!cancelling&&!confirmed)return (inventoryMode?100:30)+recallSlot;
 if(!e&&eligible)for(int i=0;i<2;i++)if(confirmationReport[i]){
    int report=650+(confirmationReport[i]-1)*2+i;confirmationReport[i]=0;return report;
 }
 return eligible?e:0;}
extern "C" __declspec(dllexport) void SetWalkStowed(int mask){
 std::lock_guard guard(lock);stowMask=walkingTest?(mask&3):0;
}
extern "C" __declspec(dllexport) int PollOffhandTrigger(int enabled){
 std::lock_guard guard(lock);
 if(!enabled || !inventoryMode || !heartbeat){offhandCapture=offhandArmed=offhandPressed=reloadReady=reloadPressed=false;return 0;}
 offhandCapture=true;int pressed=(offhandPressed?1:0)|(reloadPressed?2:0);offhandPressed=reloadPressed=false;return pressed;
}
extern "C" __declspec(dllexport) int IsInventoryOpen(){
 std::lock_guard guard(lock);auto now=GetTickCount64();
 if(!markerFrame.session || (now-hands[0].seen>500 && now-hands[1].seen>500)){
 if(inventoryOpen)closeMenu();
 }
 return inventoryMode && (inventoryOpen || (selectingCard && recallHand>=0) || now<selectionFinishUntil);
}
extern "C" __declspec(dllexport) void ResolveInventoryHip(int hand,int item){
 std::lock_guard guard(lock);
 if(!inventoryMode || inventoryOpen || recallHand>=0 || hand<0 || hand>1 || item< -2 || item>=weaponCount)return;
 if(item==-2){if(selectingCard)selectionFinishUntil=GetTickCount64()+180;selectingCard=false;return;}
 recallSlot=item;startRecall(hand,GetTickCount64());
}
extern "C" __declspec(dllexport) void WheelFeedback(int slot,int visible,int match){
 std::lock_guard guard(lock);auto now=GetTickCount64();
 if(recallHand<0 || recallSlot!=slot || !gripStarted || cancelling || confirmed)return;
 if(!visible){if(steering)cancelling=now;steering=false;matches=0;return;}
 if(!steering){steering=true;sectorSince=now;return;}
 if(now-sectorSince<40)return;
 matches=match?matches+1:0;
 if(matches>=2){confirmed=true;if(inventoryMode && slot>=0 && slot<weaponCount)lastConfirmedSector[slot]=sector;return;}
 if(now-sectorSince>=150 && !match){
    if(++sectorsTried==8){cancelling=now;steering=false;}
    else{sector=(sector+1)%8;sectorSince=now;}
 }
}
extern "C" __declspec(dllexport) XrResult XRAPI_CALL xrNegotiateLoaderApiLayerInterface(const XrNegotiateLoaderInfo* l,const char*,XrNegotiateApiLayerRequest* r){
 if(l->minInterfaceVersion>1||l->maxInterfaceVersion<1)return XR_ERROR_INITIALIZATION_FAILED;
 r->layerInterfaceVersion=1;r->layerApiVersion=XR_API_VERSION_1_0;r->getInstanceProcAddr=proc;r->createApiLayerInstance=start;return XR_SUCCESS;}
