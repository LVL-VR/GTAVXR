using Assignments=std::array<int,4>;
Assignments assigned{},draftAssigned{};
Layout draftLayout{};
bool editor=false,seeded=false;
int menuHand=-1,dragSlot=-1,editorHand=-1;
XrAction stickClick{};bool clickDown[2]{},swallowClick[2]{};
XrAction rumble{};
PFN_xrApplyHapticFeedback applyPulse{};
ULONGLONG lastPulse[2]{};
struct OverwritePulse {int remaining=0;ULONGLONG due=0,protectedUntil=0;XrSession session{};};
constexpr ULONGLONG savePulseMs=80,saveSpacingMs=200;
PFN_xrStopHapticFeedback stopPulse{};
std::array<int,2> confirmationReport{};

std::array<OverwritePulse,2> confirmations{};
bool confirmationOwns(XrSession session,const XrHapticActionInfo* info,ULONGLONG now){
 if(!info||!rumble||info->action!=rumble)return false;
 for(int i=0;i<2;i++)if(hands[i].path&&info->subactionPath==hands[i].path&&
    confirmations[i].session==session&&now<confirmations[i].protectedUntil)return true;
 return false;
}
XrResult XRAPI_CALL gamePulse(XrSession session,const XrHapticActionInfo* info,const XrHapticBaseHeader* pulse){
 std::lock_guard guard(lock);
 if(confirmationOwns(session,info,GetTickCount64()))return XR_SUCCESS;
 return applyPulse(session,info,pulse);
}
XrResult XRAPI_CALL gameStop(XrSession session,const XrHapticActionInfo* info){
 std::lock_guard guard(lock);
 if(confirmationOwns(session,info,GetTickCount64()))return XR_SUCCESS;
 return stopPulse(session,info);
}
bool vibrate(XrSession session,int hand,XrDuration duration,float amplitude){
 if(!applyPulse||!rumble||!hands[hand].path)return false;
 XrHapticActionInfo target{XR_TYPE_HAPTIC_ACTION_INFO};target.action=rumble;target.subactionPath=hands[hand].path;
 XrHapticVibration pulse{XR_TYPE_HAPTIC_VIBRATION};pulse.duration=duration;pulse.amplitude=amplitude;
 return applyPulse(session,&target,reinterpret_cast<const XrHapticBaseHeader*>(&pulse))==XR_SUCCESS;
}
void tickConfirmations(XrSession session,ULONGLONG now){
 for(int i=0;i<2;i++){
    auto& c=confirmations[i];
    if(!c.remaining||now<c.due)continue;
    if(now-c.due>450){confirmationReport[i]=2;c={};continue;}
    if(vibrate(session,i,savePulseMs*1000000,.80f)){
       lastPulse[i]=now;--c.remaining;c.due=now+saveSpacingMs;
       c.session=session;c.protectedUntil=now+(c.remaining?saveSpacingMs+savePulseMs:savePulseMs);
       if(!c.remaining)confirmationReport[i]=1;
    }
 }
}
void pulseSlot(XrSession session,int hand,int slot,ULONGLONG now){
 auto& h=hands[hand];
 if(slot<0 || (slot==h.zone && now-h.seen<175) || !inventoryMode || (inventoryOpen&&!editor) || (editor&&dragSlot>=0) ||
    confirmations[hand].remaining || now-lastPulse[hand]<120)return;
 if(vibrate(session,hand,24000000,.22f))lastPulse[hand]=now;
}
auto editorFile(){return layoutFile.parent_path()/L"four-confirmation-editor-v1.txt";}
void seedSlots(const int* initial){
 std::copy_n(initial,4,assigned.begin());seeded=true;
 if(layoutFile.empty())return;
 std::ifstream in(editorFile());Assignments items;Layout positions;
 for(int i=0;i<4;i++){auto& p=positions[i];if(!(in>>items[i]>>p.x>>p.y>>p.z)||items[i]<-1||items[i]>=weaponCount||!validCenter(p))return;}
 in>>std::ws;if(in.eof()){assigned=items;centers=positions;}
}
bool saveEditor(){
 if(!seeded || layoutFile.empty())return false;
 std::error_code error;std::filesystem::create_directories(layoutFile.parent_path(),error);if(error)return false;
 auto path=editorFile(),tmp=path;tmp+=L".tmp";std::ofstream out(tmp);out.precision(9);
 for(int i=0;i<4;i++){auto p=draftLayout[i];out<<draftAssigned[i]<<' '<<p.x<<' '<<p.y<<' '<<p.z<<'\n';}
 out.close();if(!out||!MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;
 centers=draftLayout;assigned=draftAssigned;return true;
}
void closeMenu(){
 inventoryOpen=editor=false;menuHand=editorHand=dragSlot=-1;
 for(auto& h:hands){h.held=-1;h.high=true;}
}
bool canPlace(int slot,XrVector3f point){
 if(!validCenter(point))return false;
 for(int i=0;i<4;i++)if(i!=slot){
 auto p=draftLayout[i];float x=p.x-point.x,y=p.y-point.y,z=p.z-point.z;
 if(x*x+y*y+z*z<.12f*.12f)return false;}
 return true;
}
// Lock onto the reaching hand; tolerate short tracking gaps at the hip.
int stickTarget=-1;
ULONGLONG stickSince=0,missingSince=0,enteredZone[2]{};
bool holdFinished=false;
int emptyAssignmentHands=0;
bool blockWheelGrip[2]{};
bool nearOriginalWheel(int hand,XrVector3f p){
 // GTAVXR tests the same-side ear volume on grip press. Allow for pose sampling differences.
 float side=hand?p.x:-p.x;
 return side>.025f&&side<.39f&&p.y>-.20f&&p.y<.22f&&p.z>-.065f&&p.z<.33f
        &&p.x*p.x+p.y*p.y+p.z*p.z<.43f*.43f;
}
constexpr int moveTarget=8,exitTarget=9;
void clearStickHold(){stickTarget=-1;stickSince=missingSince=0;holdFinished=false;emptyAssignmentHands=0;}
int rightStickTarget(ULONGLONG now){
 if(!inventoryMode||!heartbeat||now-heartbeat>250)return -1;
 if(editor)return exitTarget;
 if(inventoryOpen){
    if(menuHand<0)return -1;
    const auto& h=hands[menuHand];auto p=h.position;
    return now-h.seen<175&&fabsf(p.x)<.08f&&fabsf(p.y-wheelY)<.04f&&fabsf(p.z-wheelZ)<.09f?moveTarget:-1;
 }
 int target=-1;
 for(int hand=0;hand<2;hand++){
    auto& h=hands[hand];
    if(!h.seen||now-h.seen>=175||h.zone<0)continue;
    if(h.held>=0||(emptyAssignmentHands&(1<<hand)))continue;
    int candidate=4*hand+h.zone;
    if(candidate==stickTarget)return candidate;
    if(target<0||enteredZone[hand]>enteredZone[target/4])target=candidate;
    else if(enteredZone[hand]==enteredZone[target/4])return -1;
 }
 return target;
}
void rightStickHold(bool down,bool wasDown,ULONGLONG now){
 if(!down){clearStickHold();return;}
 if(!wasDown)clearStickHold();
 if(holdFinished)return;
 int target=rightStickTarget(now);
 if(target<0){
    if(stickTarget>=0&&!missingSince)missingSince=now;
    return;
 }
 if(target!=stickTarget||(missingSince&&now-missingSince>180)){
    stickTarget=target;stickSince=now;missingSince=0;
    if(target<moveTarget&&!event)event=620+target;
    return;
 }
 missingSince=0;
 if(now-stickSince<750)return;
 holdFinished=true;
 if(target==exitTarget){
    if(dragSlot>=0){
       const auto& h=hands[editorHand];
       if(!h.seen||now-h.seen>=175||!canPlace(dragSlot,h.position)){event=552;return;}
       draftLayout[dragSlot]=h.position;
    }
    if(saveEditor()){closeMenu();event=550;}else event=551;
 }else if(target==moveTarget){
    editor=true;draftLayout=centers;draftAssigned=assigned;editorHand=dragSlot=-1;
    for(auto& h:hands)h.held=-1;
    event=500;
 }else event=600+target;
}
bool fourGrip(int hand,float raw,ULONGLONG now){
 auto& h=hands[hand];bool down=raw>=.7f&&!h.high,up=raw<.25f&&h.high;
 if(raw>=.7f)h.high=true;else if(raw<.25f)h.high=false;
 bool fresh=h.seen&&now-h.seen<175;
 if(inventoryOpen){
    if(editor){
       if(down&&fresh&&dragSlot<0){
          int slot=bodySlot(h.position.x,h.position.y,h.position.z,draftLayout);
          if(slot>=0){editorHand=hand;dragSlot=slot;}
       }else if(up&&editorHand==hand&&dragSlot>=0){
          if(fresh&&canPlace(dragSlot,h.position))draftLayout[dragSlot]=h.position;else event=552;
          dragSlot=editorHand=-1;
       }
       return true;
    }
    if(hand!=menuHand||!up)return true;
    h.held=-1;
    int card=fresh?touchedCard(h.position):-1,item=card<0?-1:pageCards()[card];
    closeMenu();
    if(item>=0){selectingCard=true;selectionFinishUntil=now+180;event=1000+hand*weaponCount+item;}
    else event=60;
    return true;
 }
 if(down&&fresh&&h.zone>=0){h.held=h.zone;h.press=now;}
 if(h.held<0)return false;
 int slot=h.held;
 if(slot>=2&&raw>=.7f&&fresh&&h.zone==slot&&now-h.press>=350){
    inventoryOpen=true;menuHand=hand;inventoryPage=0;
    for(auto& other:hands)other.held=-1;
 }else if(up){
    if(now-h.press<(slot>=2?350u:650u))pendingHip[hand]=slot;
    h.held=-1;
 }
 return true;
}
