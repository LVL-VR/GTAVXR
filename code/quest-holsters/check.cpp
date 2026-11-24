#include "layer.cpp"
#include <cassert>
#include <string>
#include <cstdio>
static int pulses=0,expectedHand=1;
static XrResult XRAPI_CALL mockPulse(XrSession,const XrHapticActionInfo* target,const XrHapticBaseHeader* base){
 assert(target->subactionPath==hands[expectedHand].path&&base->type==XR_TYPE_HAPTIC_VIBRATION);++pulses;return XR_SUCCESS;
}
static float raw;
static XrVector2f sampleStick{};
static XrResult XRAPI_CALL fakeStick(XrSession,const XrActionStateGetInfo*,XrActionStateVector2f* result){
 result->isActive=XR_TRUE;result->currentState=sampleStick;return XR_SUCCESS;
}
static bool rawButton=true;
static XrResult XRAPI_CALL fakeLocation(XrSpace,XrSpace,XrTime,XrSpaceLocation* out){
 out->locationFlags=XR_SPACE_LOCATION_POSITION_TRACKED_BIT;out->pose.position={0,-.7f,0};return XR_SUCCESS;}
static XrResult XRAPI_CALL fakeButton(XrSession,const XrActionStateGetInfo*,XrActionStateBoolean* out){out->isActive=true;out->currentState=rawButton;return XR_SUCCESS;}
static XrResult XRAPI_CALL fakeRead(XrSession,const XrActionStateGetInfo*,XrActionStateFloat* out){out->currentState=raw;out->isActive=true;return XR_SUCCESS;}
int wmain(int argc,wchar_t** argv){
 if(argc!=2)return 1;
 assert(ammoText({21,261})==L"21/240" && ammoText({0,40})==L"0/40");
 assert(ammoText({-1,0})==L"0/0" && ammoText({-1,40})==L"40");
 assert(ammoText({2,1})==L"1" && ammoText({}).empty());
 available.fill(1);
 for(int k=0;k<8;k++){
    int first=pageCards()[k];assert(first>=0);int count=0;
    do{assert(catalog[pageCards()[k]].category==k);++count;cycleCategory(k);assert(count<=weaponCount);}while(pageCards()[k]!=first);
    assert(count==std::count_if(std::begin(catalog),std::end(catalog),[k](auto w){return w.category==k;}));
 }
 ReportCardAmmo(0,21,261);auto w=wheelState(GetTickCount64());
 std::vector<uint32_t> preview(atlasWidth*atlasHeight);assert(paintWheel(preview,w));
 assert(w.clip[0]==21&&w.total[0]==261&&w.count[0]>1);
#ifdef WITH_HUD_ART
 assert(hudIcons()[0].rgba&&hudIcons()[4].rgba);
 assert(std::count_if(hudIcons().begin(),hudIcons().end(),[](HudIcon i){return i.rgba!=nullptr;})==108);
#endif
 auto orbCenter=(wheelHeight+pixels/2)*atlasWidth+pixels/2;
 assert(preview[orbCenter]==0);
 auto fittingPreview=w;fittingPreview.items[8]=doneCard;
 assert(paintWheel(preview,fittingPreview)&&preview[orbCenter]>>24==255);
 assert((preview[wheelHeight*atlasWidth]>>24)==0);
 assert(paintWheel(preview,w)&&preview[orbCenter]==0);
 assert(preview[0]==0&&preview[wheelHeight/2*atlasWidth+atlasWidth/2]==0);
 auto opacity=preview[(wheelHeight/2-210)*atlasWidth+atlasWidth/2]>>24;assert(opacity>0&&opacity<255);
 for(int k=0;k<8;k++)assert(touchedCard(cards[k])==k);
 assert(touchedCard(cards[8])==-1);
 auto original=preview;w.hover=0;assert(paintWheel(preview,w)&&preview!=original);
 available.fill(0);for(int i=0;i<5;i++)available[i]=1;
 puts("Full catalog, eight sectors, cycle wrap, ammo and radial rendering passed");
 auto loader=LoadLibraryW(argv[1]);if(!loader)return 2;
 auto enumerate=(PFN_xrEnumerateApiLayerProperties)GetProcAddress(loader,"xrEnumerateApiLayerProperties");
 XrApiLayerProperties props[64]{};for(auto& p:props)p.type=XR_TYPE_API_LAYER_PROPERTIES;
 uint32_t count=0;assert(enumerate(64,&count,props)==XR_SUCCESS);bool found=false;
 for(unsigned i=0;i<count;i++)if(!strcmp(props[i].layerName,"XR_APILAYER_GTAVXR_quest_holsters"))found=true;
 printf("Layer discoverable: %d\n",found);
 readFloat=fakeRead;grip=(XrAction)1;hands[0].path=1;
 for(float yaw:{0.f,1.2f,-2.7f})for(int slot=0;slot<4;slot++){
 MarkerFrame f{{},{},0,{2.f,1.65f,-3.f},cosf(yaw),sinf(yaw)};
 auto m=markerAt(f,centers[slot]);auto dx=m.position.x-f.head.x,dz=m.position.z-f.head.z;
 assert(bodySlot(f.c*dx-f.s*dz,m.position.y-f.head.y,f.s*dx+f.c*dz)==slot);
 auto q=m.orientation;assert(fabsf(q.x*q.x+q.y*q.y+q.w*q.w-1)<.0001f);}
 for(int i=0;i<2;i++){float side=i?1.f:-1.f;
 assert(bodySlot(side*.15f,-.28f,-.18f)==-1);
 assert(bodySlot(side*.24f,-.43f,-.42f)==-1);
 assert(bodySlot(side*.24f,-.49f,-.42f)==-1);
 assert(bodySlot(side*.24f,-.53f,-.39f)==-1);
 assert(bodySlot(side*.24f,-.56f,-.34f)==i);
 assert(bodySlot(side*.07f,.01f,.12f)==-1);
 assert(bodySlot(side*.44f,-.74f,.03f)==i);
 assert(bodySlot(side*.17f,-.12f,.08f)==-1);
 assert(bodySlot(side*.17f,-.09f,.08f)==2+i);
 assert(bodySlot(side*.21f,-.27f,.10f)==-1);}
 XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};info.action=grip;info.subactionPath=1;
 XrActionStateFloat out{XR_TYPE_ACTION_STATE_FLOAT};
 hands[1].path=2;
 for(int hand=0;hand<2;hand++)for(int z=0;z<4;z++)for(int hold=0;hold<2;hold++){
 info.subactionPath=hands[hand].path;
 PollHolster(1);auto& h=hands[hand];auto target=centers[z];h.zone=bodySlot(target.x,target.y,target.z);h.seen=GetTickCount64();
 raw=0;value({},&info,&out);raw=1;value({},&info,&out);assert(out.currentState==0);
 if(hold){h.press-=700;value({},&info,&out);
 if(z==2+hand){assert(out.currentState==1);raw=0;value({},&info,&out);assert(PollHolster(1)==0);continue;}
 assert(out.currentState==0);}raw=0;value({},&info,&out);assert(PollHolster(1)==1+z+4*hold);assert(PollHolster(1)==0);}
 locateSpace=fakeLocation;markerFrame={{},(XrSpace)4,0,{0,0,0},1,0};
 for(int hand=0;hand<2;hand++)for(int hip=0;hip<2;hip++){
   PollHolster(7);auto& h=hands[hand];h.space=(XrSpace)(uintptr_t)(hand+1);
   h.zone=hip;h.seen=GetTickCount64();info.subactionPath=h.path;
   raw=1;value({},&info,&out);raw=0;value({},&info,&out);
   assert(recallHand==hand && PollHolster(7)==0);
   value({},&info,&out);assert(out.currentState==0);
   XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};handLocation(h.space,markerFrame.space,0,&loc);
   assert(poseReady && loc.pose.position.x==(hand?.20f:-.20f));
   poseReady-=17;value({},&info,&out);assert(out.currentState==1);
   assert(PollHolster(7)==30+hip);
   WheelFeedback(hip,0,1);assert(!steering&&!confirmed);
   WheelFeedback(hip,1,0);sectorSince-=160;WheelFeedback(hip,1,0);assert(sector==1&&!confirmed);
   sectorSince-=50;WheelFeedback(hip,1,1);assert(!confirmed);WheelFeedback(hip,1,1);assert(confirmed);
   value({},&info,&out);assert(out.currentState==0 && recallHand==-1);
   handLocation(h.space,markerFrame.space,0,&loc);assert(loc.pose.position.y==-.7f);
 }
 for(int hand=0;hand<2;hand++)for(int slot=0;slot<4;slot++){
   PollHolster(9);auto& h=hands[hand];info.subactionPath=h.path;h.zone=slot;h.seen=GetTickCount64();
   raw=0;value({},&info,&out);raw=1;value({},&info,&out);assert(out.currentState==(slot<2?0:1));
   raw=0;value({},&info,&out);assert(PollHolster(9)==(slot<2?40+hand:0));assert(recallHand==-1);
 }
 walkTrigger=(XrAction)3;info.action=walkTrigger;
 for(int hand=0;hand<2;hand++){
    info.action=grip;info.subactionPath=hands[hand].path;auto& h=hands[hand];
    PollHolster(9);h.zone=1-hand;h.seen=GetTickCount64()-120;
    raw=0;value({},&info,&out);raw=1;value({},&info,&out);
    HipPress press{};assert(TakeHipPress(hand,&press)&&press.zone==1-hand);
    h.zone=-1;raw=0;value({},&info,&out);assert(PollHolster(9)==40+hand);
    h.zone=hand;h.seen=GetTickCount64()-400;raw=1;value({},&info,&out);
    raw=0;value({},&info,&out);assert(PollHolster(9)==0);
 }
 info.action=walkTrigger;
 for(int hand=0;hand<2;hand++)for(int hip=0;hip<2;hip++){
    PollHolster(73);auto& h=hands[hand];h.zone=hip;h.seen=GetTickCount64();
    info.action=grip;info.subactionPath=h.path;
    raw=0;value({},&info,&out);raw=1;value({},&info,&out);raw=0;value({},&info,&out);
    assert(recallHand==hand && PollHolster(73)==0);
    XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};handLocation(h.space,markerFrame.space,0,&loc);
    assert(poseReady && loc.pose.position.x==(hand?.20f:-.20f));
    poseReady-=17;value({},&info,&out);assert(out.currentState==1);
    gripStarted-=66;value({},&info,&out);assert(out.currentState==0 && recallHand==-1);
    assert(PollHolster(73)==40+hand && PollHolster(73)==0);
 }
 PollHolster(201|(31<<8));info.action=grip;info.subactionPath=hands[0].path;
 auto& picker=hands[0];picker.zone=2;picker.seen=GetTickCount64();
 raw=0;value({},&info,&out);raw=1;value({},&info,&out);picker.press-=351;value({},&info,&out);
 assert(inventoryOpen && out.currentState==0);
 picker.position=cards[0];stickClick=(XrAction)300;readBoolean=fakeButton;
 auto clickInfo=info;clickInfo.action=stickClick;XrActionStateBoolean clickState{XR_TYPE_ACTION_STATE_BOOLEAN};
 clickInfo.subactionPath=hands[1].path;rawButton=true;button({},&clickInfo,&clickState);
 assert(clickState.currentState&&pageCards()[0]==0);clickInfo.subactionPath=picker.path;
 rawButton=false;button({},&clickInfo,&clickState);rawButton=true;button({},&clickInfo,&clickState);
 assert(!clickState.currentState&&pageCards()[0]==1);button({},&clickInfo,&clickState);assert(pageCards()[0]==1);
 rawButton=false;button({},&clickInfo,&clickState);selected[0]=0;
 picker.position=cards[5];raw=0;value({},&info,&out);
 const int shotgunEvent=1004;
 assert(!inventoryOpen && recallHand==-1 && PollHolster(201|(31<<8))==shotgunEvent);
 ResolveInventoryHip(0,4);assert(recallHand==0 && recallSlot==4);
 poseReady=GetTickCount64()-17;value({},&info,&out);assert(out.currentState==1);
 markerFrame.session=(XrSession)1;
 assert(PollHolster(201|(31<<8))==104 && IsInventoryOpen());
 WheelFeedback(4,1,0);sectorSince-=151;WheelFeedback(4,1,0);assert(sector==1);
 sectorSince-=50;WheelFeedback(4,1,1);WheelFeedback(4,1,1);assert(confirmed);
 value({},&info,&out);assert(recallHand==-1 && IsInventoryOpen());
 ResolveInventoryHip(1,4);assert(sector==1);gripStarted=GetTickCount64();WheelFeedback(4,1,0);
 for(int n=1;n<8;n++){sectorSince-=151;WheelFeedback(4,1,0);assert(sector==(1+n)%8&&!cancelling);}
 sectorSince-=151;WheelFeedback(4,1,0);assert(cancelling);startRecall(-1,GetTickCount64());
 selectionFinishUntil=0;assert(!IsInventoryOpen());
 for(int hand=0;hand<2;hand++)for(int slot=0;slot<4;slot++){
    auto& h=hands[hand];info.subactionPath=h.path;h.zone=slot;h.seen=GetTickCount64();
    raw=0;value({},&info,&out);raw=1;value({},&info,&out);
    h.zone=-1;raw=0;value({},&info,&out);
    assert(PollHolster(201|(31<<8))==70+hand*4+slot);
    assert(recallHand==-1 && !inventoryOpen);
    h.zone=slot;h.seen=GetTickCount64();
    raw=0;value({},&info,&out);raw=1;value({},&info,&out);raw=0;value({},&info,&out);
    assert(PollHolster(201|(31<<8))==70+hand*4+slot);
    ResolveInventoryHip(hand,0);assert(recallHand==hand && recallSlot==0);startRecall(-1,GetTickCount64());
    if(slot!=2+hand){
       raw=1;value({},&info,&out);h.press-=1050;raw=0;value({},&info,&out);
       assert(PollHolster(201|(31<<8))==0);
    }
 }
 {
 applyPulse=mockPulse;rumble=(XrAction)20;
 auto stamp=GetTickCount64();hands[1].zone=-1;pulseSlot({},1,0,stamp);hands[1].zone=0;hands[1].seen=stamp+150;
 pulseSlot({},1,0,stamp+150);assert(pulses==1);
 pulseSlot({},1,1,stamp+200);assert(pulses==2);applyPulse=nullptr;rumble={};
 auto previousFile=layoutFile;auto originalCenters=centers;
 layoutFile=std::filesystem::temp_directory_path()/("four-editor-check-"+std::to_string(GetCurrentProcessId()))/"positions.txt";
 int initial[]={0,0,4,4};SyncSlots(initial);
 auto sample=[&](int hand,float gripValue){auto& h=hands[hand];info.subactionPath=h.path;h.seen=GetTickCount64();raw=gripValue;value({},&info,&out);};
 auto open=[&](){hands[1].zone=2;sample(1,0);sample(1,1);hands[1].press-=360;sample(1,1);assert(inventoryOpen&&menuHand==1);};
 auto tap=[&](int hand,XrVector3f p){hands[hand].position=p;sample(hand,0);sample(hand,1);sample(hand,0);};
 auto pollTest=[](){return PollHolster(201|(31<<8));};
 open();tap(0,cards[0]);assert(inventoryOpen&&pollTest()==0);
 hands[1].position={0,0,0};sample(1,0);assert(!inventoryOpen&&pollTest()==60);
 open();hands[1].position=cards[0];sample(1,0);assert(pollTest()==1000+weaponCount);ResolveInventoryHip(1,-2);
 hands[0].zone=2;hands[1].zone=-1;
 // Inside the stock shoulder volume but outside the smaller calibrated target.
 hands[1].position={.07f,.03f,.10f};sample(1,0);sample(1,1);assert(out.currentState==0);
 hands[1].position={.4f,-.4f,-.45f};sample(1,1);assert(out.currentState==0);
 sample(1,0);sample(1,1);assert(out.currentState==1);sample(1,0);assert(pollTest()==0);
 auto retargetTime=GetTickCount64();heartbeat=hands[0].seen=hands[1].seen=retargetTime;
 hands[0].zone=0;hands[1].zone=-1;rightStickHold(true,false,retargetTime);assert(pollTest()==620);
 SkipEmptyAssignmentHand(0);hands[1].zone=2;rightStickHold(true,true,retargetTime);assert(pollTest()==626);
 heartbeat=hands[0].seen=hands[1].seen=retargetTime+800;
 rightStickHold(true,true,retargetTime+800);assert(pollTest()==606);
 rightStickHold(false,true,retargetTime+800);hands[0].zone=2;hands[1].zone=-1;
 neutralUntil[1]=0; // Prior simulated draw finished its stick-neutral interval.
 wheelStick=(XrAction)305;readVector=fakeStick;
 auto axisInfo=info;axisInfo.action=wheelStick;axisInfo.subactionPath=hands[1].path;
 XrActionStateVector2f axis{XR_TYPE_ACTION_STATE_VECTOR2F};
 auto axisSample=[&](float x,float y){sampleStick={x,y};hands[0].seen=GetTickCount64();wheelVector({},&axisInfo,&axis);};
 axisSample(0,1);assert(axis.currentState.y==1&&pollTest()==0);
 axisSample(.85f,-.8f);assert(axis.currentState.x==.85f&&pollTest()==0);
 clickInfo.subactionPath=hands[1].path;rawButton=true;button({},&clickInfo,&clickState);
 assert(clickState.currentState&&pollTest()==0);rawButton=false;button({},&clickInfo,&clickState);
 axisSample(0,-.86f);assert(axis.currentState.y==0&&pollTest()==622);
 stickSince-=760;axisSample(0,-.68f);assert(pollTest()==602);
 axisSample(0,-1);assert(pollTest()==0);axisSample(0,0);
 auto holdTime=GetTickCount64();heartbeat=hands[0].seen=holdTime;
 rightStickHold(true,false,holdTime);assert(pollTest()==622);rightStickHold(false,true,holdTime+100);assert(pollTest()==0);
 rightStickHold(true,false,holdTime);assert(pollTest()==622);hands[0].zone=-1;rightStickHold(true,true,holdTime+50);
 hands[0].zone=2;heartbeat=hands[0].seen=holdTime+800;rightStickHold(true,true,holdTime+800);assert(pollTest()==622);
 rightStickHold(false,true,holdTime+800);
 holdTime=GetTickCount64();heartbeat=hands[0].seen=holdTime;
 hands[0].zone=-1;rightStickHold(true,false,holdTime);assert(pollTest()==0);
 hands[0].zone=2;rightStickHold(true,true,holdTime);assert(pollTest()==622);
 hands[0].zone=-1;rightStickHold(true,true,holdTime+150);
 heartbeat=hands[0].seen=holdTime+240;hands[0].zone=2;rightStickHold(true,true,holdTime+240);
 hands[1].zone=1;hands[1].seen=holdTime+800;
 heartbeat=hands[0].seen=holdTime+800;rightStickHold(true,true,holdTime+800);assert(pollTest()==602);
 rightStickHold(true,true,holdTime+810);assert(pollTest()==0);rightStickHold(false,true,holdTime+810);
 for(int hand=0;hand<2;hand++){
    confirmations={};expectedHand=hand;applyPulse=mockPulse;rumble=(XrAction)20;int before=pulses;
    assert(SaveSlotForHand(hand,2,1)&&assigned[2]==1);
    auto due=confirmations[hand].due;tickConfirmations({},due);tickConfirmations({},due+50);assert(pulses==before+1);
    XrHapticActionInfo haptic{XR_TYPE_HAPTIC_ACTION_INFO};haptic.action=rumble;haptic.subactionPath=hands[hand].path;
    assert(confirmationOwns({},&haptic,due+90));haptic.subactionPath=hands[1-hand].path;assert(!confirmationOwns({},&haptic,due+90));
    tickConfirmations({},due+199);assert(pulses==before+1);
    tickConfirmations({},due+200);tickConfirmations({},due+300);assert(pulses==before+2);
    assert(pollTest()==650+hand);
    assert(!SaveSlotForHand(hand,2,-1)&&assigned[2]==1&&!confirmations[hand].remaining);
 }
 auto actualPath=layoutFile;layoutFile.clear();assert(!SaveSlotForHand(0,2,0)&&assigned[2]==1&&!confirmations[0].remaining);layoutFile=actualPath;
 applyPulse=nullptr;rumble={};
 open();hands[1].position=cards[8];holdTime=GetTickCount64();heartbeat=hands[1].seen=holdTime;
 assert(pageCards()[8]<0);rightStickHold(true,false,holdTime);heartbeat=hands[1].seen=holdTime+800;
 rightStickHold(true,true,holdTime+800);assert(editor&&pollTest()==500&&pageCards()[0]<0);
 rightStickHold(false,true,holdTime+800);sample(1,0);
 auto p=draftLayout[0];p.x-=.04f;hands[0].position=draftLayout[0];sample(0,0);sample(0,1);assert(dragSlot==0);
 hands[0].position=p;sample(0,0);assert(draftLayout[0].x==p.x&&centers[0].x!=p.x);
 tap(1,cards[8]);assert(editor&&pollTest()==0);
 hands[0].position=p;sample(0,1);assert(dragSlot==0);
 hands[0].position.x+=.01f;p=hands[0].position;
 holdTime=GetTickCount64();heartbeat=hands[0].seen=holdTime;
 rightStickHold(true,false,holdTime);heartbeat=hands[0].seen=holdTime+800;
 rightStickHold(true,true,holdTime+800);assert(!editor&&!inventoryOpen&&pollTest()==550&&centers[0].x==p.x);
 sample(0,0);sample(1,0);assert(pollTest()==0);seeded=false;centers=originalCenters;SyncSlots(initial);assert(initial[2]==1&&centers[0].x==p.x);
 std::filesystem::remove(editorFile());std::filesystem::remove(layoutFile.parent_path());
 centers=originalCenters;layoutFile=previousFile;seeded=false;
 puts("Shoulder release selection, cancellation, right-stick assignment, drag, save and reload passed");
 }
 info.action=walkTrigger;
 PollHolster(201);info.subactionPath=hands[0].path;PollOffhandTrigger(1);
 raw=0;value({},&info,&out);raw=1;value({},&info,&out);
 assert(out.currentState==0 && PollOffhandTrigger(1)==1);
 value({},&info,&out);assert(PollOffhandTrigger(1)==0);
 info.subactionPath=hands[1].path;value({},&info,&out);assert(out.currentState==1);
 offhandPrimary=(XrAction)99;readBoolean=fakeButton;auto reloadInfo=info;
 reloadInfo.action=offhandPrimary;reloadInfo.subactionPath=hands[0].path;
 XrActionStateBoolean reloadState{XR_TYPE_ACTION_STATE_BOOLEAN};
 rawButton=false;button({},&reloadInfo,&reloadState);
 rawButton=true;button({},&reloadInfo,&reloadState);
 assert(!reloadState.currentState && PollOffhandTrigger(1)==2);
 button({},&reloadInfo,&reloadState);assert(PollOffhandTrigger(1)==0);
 reloadInfo.subactionPath=hands[1].path;button({},&reloadInfo,&reloadState);assert(reloadState.currentState);
 PollOffhandTrigger(0);info.subactionPath=hands[0].path;raw=0;value({},&info,&out);
 for(int mask=0;mask<4;mask++){
    PollHolster(9);SetWalkStowed(mask);
    for(int i=0;i<2;i++){info.subactionPath=hands[i].path;raw=0;value({},&info,&out);
       raw=1;value({},&info,&out);assert(out.currentState==((mask&(1<<i))?0:1));}
 }
 SetWalkStowed(0);raw=1;value({},&info,&out);assert(out.currentState==0);
 raw=0;value({},&info,&out);raw=1;value({},&info,&out);assert(out.currentState==1);
 info.action=grip;
 PollHolster(1);info.subactionPath=hands[0].path;
 hands[0].zone=-1;raw=1;value({},&info,&out);assert(out.currentState==1);
 if(argc==2){layoutFile=std::filesystem::temp_directory_path()/("holster-check-"+std::to_string(GetCurrentProcessId()))/"positions.txt";
 editSlot=0;hands[0].position={-.15f,-.35f,-.52f};raw=0;value({},&info,&out);
 raw=1;value({},&info,&out);assert(dragHand==0);raw=0;value({},&info,&out);assert(editSlot==-1);
 assert(PollHolster(1)==0);centers=defaultLayout();assert(readLayout(layoutFile,centers));
 assert(bodySlot(-.15f,-.35f,-.52f)==0);assert(centers[0].y==hands[0].position.y);
 assert(centers[3].x==defaultLayout()[3].x);
 resetFitting();auto press=[&](int i,float v){info.subactionPath=hands[i].path;hands[i].seen=GetTickCount64();raw=v;value({},&info,&out);};
 auto toggle=[&](){PollHolster(1);hands[0].position={-.06f,-.23f,-.42f};hands[1].position={.06f,-.23f,-.42f};
 press(0,1);press(1,1);assert(chordSince);chordSince-=1501;press(0,1);press(0,0);press(1,0);};
 toggle();assert(fitting&&PollHolster(1)==9&&!waitRelease);
 hands[0].position={-.40f,-.70f,0};hands[1].position=centers[0];press(1,1);assert(dragHand==1&&editSlot==0&&PollHolster(1)==20);
 hands[1].position={-.23f,-.58f,-.30f};press(1,0);assert(PollHolster(1)==12&&readLayout(layoutFile,centers));
 assert(centers[0].x==-.23f);
 hands[0].position={-.06f,-.23f,-.42f};hands[1].position={.06f,-.23f,-.42f};
 press(0,1);assert(dragHand==-1);press(0,0);assert(PollHolster(1)==0);
 toggle();assert(!fitting&&PollHolster(1)==10);
 toggle();assert(fitting&&PollHolster(1)==9);
 hands[0].position={-.40f,-.70f,0};hands[1].position=centers[0];press(1,1);assert(dragHand==1);
 hands[1].position={.25f,-.45f,-.2f};
 secondary=(XrAction)2;readBoolean=fakeButton;auto key=info;key.action=secondary;key.subactionPath=hands[0].path;
 XrActionStateBoolean keyState{XR_TYPE_ACTION_STATE_BOOLEAN};button({},&key,&keyState);
 assert(!keyState.currentState&&!fitting&&dragHand==-1&&PollHolster(1)==10);
 press(0,0);assert(waitRelease);press(1,0);assert(!waitRelease&&PollHolster(1)==0);
 Layout saved;assert(readLayout(layoutFile,saved)&&saved[0].x==-.23f);
 std::filesystem::remove(layoutFile);std::filesystem::remove(layoutFile.parent_path());}
 puts("Slot move, persistence, gestures and passthrough passed");return found?0:3;
}
