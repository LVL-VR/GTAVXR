#pragma once
// retained Menyoo Vehicle Spawner (curtis VehicleSpawner.cpp: SpawnVehicle, SpawnVehicleMenu, Spawn Settings,
// AddVehicleSpawnOption, Random Vehicle, DLC Vehicles tables; FunnyVehicles.cpp: the 37 Badly Constructed Vehicles; vehicle
// preview sprites). REMOVE rows (Search, Favourites, Saved Vehicles, Input Model) have no row, page or dispatch here.
// Vehicles are created by the Trainer's OWN script thread with the ORIGINAL CREATE_VEHICLE, exactly as Menyoo does, so
// Delete Vehicle / Delete Old / Persistent keep their script-entity semantics. Before creation the EXE's accepted model
// material guard runs, and after creation the instance guard plus the offline MPBitset exemption (SourceHostVehicleGuard).
// Every step that Menyoo blocks with Model::Load/WAIT is a non-blocking job advanced once per UPDATE by tick().
// VR adaptations: with Auto-Sit off and no current vehicle, the new vehicle is placed just ahead of the player instead of
// on top of them; Spawn At Nearest Node stops at the nearest unobstructed node (the source loop overwrote it 39 times).
#include "trainer_world.h"
#include "vehicle_actions.h"
#include "vehicle_customs.h"
#include "funny_vehicles_data.h"
#include "vehicle_spawner_data.h"
#include "menu_transport.h"
#include <map>
#include <stdio.h>
#include <stdlib.h>
namespace menyoo_vr {
struct SpawnerPageIds {int settings,funny,category,dlc,dlcItems,paints2,rgb;};
// Menyoo SetVehicleMaxUpgrades (VehicleModShop.cpp), shared by Spawn Settings and the Funny Vehicles Upgrade op.
inline void spawnMaxUpgrades(int v,bool upgrade,bool invincible,int plateType,const char*plateText,bool neon,int r,int g,int b,int primary,int secondary){
 if(!tw::exists(v)||!sn::IS_ENTITY_A_VEHICLE(v))return;
 if(sn::GET_VEHICLE_MOD_KIT(v)!=0)sn::SET_VEHICLE_MOD_KIT(v,0);
 tw::setInvincible(v,invincible);
 if(plateText&&*plateText)sn::SET_VEHICLE_NUMBER_PLATE_TEXT(v,plateText);
 sn::SET_VEHICLE_NUMBER_PLATE_TEXT_INDEX(v,plateType);
 sn::TOGGLE_VEHICLE_MOD(v,18,upgrade);sn::TOGGLE_VEHICLE_MOD(v,20,upgrade);sn::TOGGLE_VEHICLE_MOD(v,22,upgrade);
 for(int i=0;i<49;++i){if(i>=17&&i<=22)continue;
  if(i==24){const int front=sn::GET_VEHICLE_MOD(v,23);sn::SET_VEHICLE_MOD(v,i,upgrade?front:-1,false);continue;}
  int index=sn::GET_NUM_VEHICLE_MODS(v,i)-1;if(index>-1)index=rand()%(index+2)-1;if(i>=11&&i<=16)index=sn::GET_NUM_VEHICLE_MODS(v,i)-1;
  sn::SET_VEHICLE_MOD(v,i,upgrade?index:-1,false);}
 sn::SET_VEHICLE_WINDOW_TINT(v,upgrade?1:0);sn::SET_VEHICLE_TYRES_CAN_BURST(v,false);
 if(neon){for(int i=0;i<=3;++i)sn::SET_VEHICLE_NEON_ENABLED(v,i,true);sn::SET_VEHICLE_NEON_COLOUR(v,r,g,b);}
 int extra1=0,extra2=0,a=0,c=0;sn::GET_VEHICLE_EXTRA_COLOURS(v,&extra1,&extra2);
 if(primary!=-3){sn::CLEAR_VEHICLE_CUSTOM_PRIMARY_COLOUR(v);sn::GET_VEHICLE_COLOURS(v,&a,&c);sn::SET_VEHICLE_COLOURS(v,primary,c);sn::SET_VEHICLE_EXTRA_COLOURS(v,0,extra2);}
 if(secondary!=-3){sn::CLEAR_VEHICLE_CUSTOM_SECONDARY_COLOUR(v);sn::GET_VEHICLE_COLOURS(v,&a,&c);sn::SET_VEHICLE_COLOURS(v,a,secondary);sn::SET_VEHICLE_EXTRA_COLOURS(v,0,extra2);}
}
class VehicleSpawner {
 SpawnerPageIds ids_;VehicleActions*catalogue_;VehicleCustoms*customs_;
 bool deleteOld_,autoSit_,blip_,nearest_,upgraded_,invincible_,persistent_,neon_,previews_;
 int primary_,secondary_,neonRgb_[3],plateType_,plateText_;
 int view_,dlc_;std::vector<unsigned>views_[25];bool viewsBuilt_;std::map<uint32_t,std::string>names_;
 enum Phase { Idle,Load,Create,LeaveOld,DeleteOld,Finish,FunnyOp,FunnyAnim };
 struct Job {int phase;uint32_t hash;int recipe;unsigned op;int started,vehicle,old,radio,anim,attempts;bool settings,deleteOld,warp,oldEngine;tw::V3 node,velocity;};
 Job job_;
 // Menyoo spawner category order: index into the class catalogue (0..22), 23 = Drift (model names starting with "drift").
 struct View {const char*name;int group;};
 static const View&view(unsigned i){static const View v[24]={{"Open Wheel",22},{"Super",7},{"Sports",6},{"Sports Classics",5},{"Coupes",3},
  {"Muscle",4},{"Offroad",9},{"SUVs",2},{"Sedans",1},{"Compacts",0},{"Drift",23},
  {"Vans",12},{"Services",17},{"Industrial",10},{"Commercial",20},{"Utility",11},{"Trains",21},
  {"Emergency",18},{"Military",19},{"Motorcycles",8},{"Bicycles",13},{"Planes",16},{"Helicopters",15},{"Boats",14}};return v[i<24?i:0];}
 static const char*plateTypeLabel(int i){static const char*l[13]={"CMOD_PLA_0","CMOD_PLA_4","CMOD_PLA_3","CMOD_PLA_1","CMOD_PLA_2","Yankton",
  "CMOD_PLA_6","CMOD_PLA_7","CMOD_PLA_8","CMOD_PLA_9","CMOD_PLA_10","CMOD_PLA_11","CMOD_PLA_12"};return l[i>=0&&i<13?i:5];}
 static int randomSprite(){static const int s[10]={396,303,304,397,394,462,206,161,42,3};return s[rand()%10];}
 void buildViews(){
  if(viewsBuilt_||!catalogue_||!catalogue_->load())return;for(unsigned i=0;i<25;++i)views_[i].clear();
  for(unsigned g=0;g<23;++g)for(unsigned i=0;i<catalogue_->count(g);++i){const CatalogueEntry*e=catalogue_->entry(g,i);if(!e)continue;
   views_[_strnicmp(e->name,"drift",5)==0?23:g].push_back(g<<16|i);}
  viewsBuilt_=true;
 }
 const CatalogueEntry*viewEntry(unsigned group,unsigned i)const{if(group>=24||i>=views_[group].size()||!catalogue_)return NULL;
  return catalogue_->entry(views_[group][i]>>16,views_[group][i]&0xffffu);}
 const std::string&displayName(uint32_t hash,const char*model){
  std::map<uint32_t,std::string>::iterator it=names_.find(hash);if(it!=names_.end())return it->second;
  const char*label=sn::GET_DISPLAY_NAME_FROM_VEHICLE_MODEL(static_cast<int>(hash));
  std::string name=label&&*label&&strcmp(label,"CARNOTFOUND")?tw::text(label,model):std::string(model);
  return names_[hash]=name;
 }
 const spawner::Preview*preview(uint32_t hash)const{
  size_t lo=0,hi=sizeof(spawner::PREVIEWS)/sizeof(spawner::PREVIEWS[0]);
  while(lo<hi){const size_t mid=(lo+hi)/2;if(spawner::PREVIEWS[mid].hash<hash)lo=mid+1;else hi=mid;}
  return lo<sizeof(spawner::PREVIEWS)/sizeof(spawner::PREVIEWS[0])&&spawner::PREVIEWS[lo].hash==hash?&spawner::PREVIEWS[lo]:NULL;
 }
 void previewRow(Transport&m,uint32_t hash){if(!previews_)return;const spawner::Preview*p=preview(hash);m.preview(p?p->dict:"",p?p->texture:"");}
 bool guard(unsigned mode,uint64_t value){uint64_t a[2]={mode,value},r[3]={};return sn::call(SourceHostVehicleGuard,a,2,r);}
 void deleteOwn(int v){int h=v;if(tw::exists(h)){sn::SET_ENTITY_AS_MISSION_ENTITY(h,true,true);sn::DELETE_VEHICLE(&h);}}
 void fail(const char*why){if(job_.hash)sn::SET_MODEL_AS_NO_LONGER_NEEDED(static_cast<int>(job_.hash));tw::notify(why);job_=Job();}
 // Menyoo SpawnVehicle up to its first WAIT(0).
 void create(){
  const int ped=tw::ped();const unsigned model=job_.hash;
  if(!guard(GuardModel,model)){fail("This model has no compatible native vehicle material; nothing was created.");return;}
  tw::V3 front,back,oldFront,oldBack;tw::extents(model,front,back);tw::V3 at;
  job_.old=tw::vehicle();
  if(job_.old){job_.velocity=sn::GET_ENTITY_VELOCITY(job_.old);job_.oldEngine=sn::GET_IS_VEHICLE_ENGINE_RUNNING(job_.old);
   tw::extents(tw::model(job_.old),oldFront,oldBack);
   at=job_.deleteOld?tw::offset(job_.old,0,0,.5f):tw::offset(job_.old,0,oldFront.y+back.y+1.f,.5f);}
  else at=job_.warp?tw::position(ped):tw::offset(ped,0,back.y+1.5f,0);
  if(sn::HAS_NAMED_PTFX_ASSET_LOADED("proj_xmas_firework")){sn::USE_PARTICLE_FX_ASSET("proj_xmas_firework");
   sn::START_PARTICLE_FX_NON_LOOPED_AT_COORD("scr_firework_xmas_burst_rgw",at,tw::v3(0,0,0),1.f,false,false,false);}
  const int v=sn::CREATE_VEHICLE(static_cast<int>(model),at,sn::GET_ENTITY_HEADING(ped),false,false,false);
  if(!v){fail("Vehicle could not be created (native vehicle limit or invalid position).");return;}
  if(!guard(GuardInstance,static_cast<uint32_t>(v))){deleteOwn(v);fail("Vehicle rejected by the native material guard; it was removed.");return;}
  job_.vehicle=v;sn::SET_ENTITY_ALPHA(v,0,false);sn::SET_ENTITY_COLLISION(v,false,true);sn::SET_VEHICLE_DIRT_LEVEL(v,0.f);
  const int old=job_.old;
  if(old&&tw::exists(old)){
   sn::SET_VEHICLE_ENGINE_ON(v,job_.oldEngine,true,false);sn::SET_ENTITY_COLLISION(v,true,true);
   sn::SET_VEHICLE_FORWARD_SPEED(v,fabsf(job_.velocity.y));sn::SET_ENTITY_VELOCITY(v,job_.velocity);
   if(job_.deleteOld){sn::SET_ENTITY_COLLISION(old,false,true);sn::SET_ENTITY_ALPHA(old,0,false);}
   sn::RESET_ENTITY_ALPHA(v);
   if(job_.warp){const int maximum=sn::GET_VEHICLE_MAX_NUMBER_OF_PASSENGERS(v);
    for(int i=-1;i<=maximum;++i){const int p=sn::GET_PED_IN_VEHICLE_SEAT(old,i,false);if(tw::exists(p))sn::SET_PED_INTO_VEHICLE(p,v,i);}}
   job_.phase=job_.deleteOld?LeaveOld:Finish;
  }else{
   if(job_.warp)sn::SET_PED_INTO_VEHICLE(ped,v,tw::firstFreeSeat(v,-1));
   sn::SET_ENTITY_COLLISION(v,true,true);sn::RESET_ENTITY_ALPHA(v);job_.phase=Finish;
  }
 }
 void leaveOld(){const int old=job_.old;if(tw::exists(old)){const int maximum=sn::GET_VEHICLE_MAX_NUMBER_OF_PASSENGERS(old);
   for(int i=-1;i<=maximum;++i){const int p=sn::GET_PED_IN_VEHICLE_SEAT(old,i,false);if(tw::exists(p)){sn::TASK_LEAVE_ANY_VEHICLE(p,0,0);sn::CLEAR_PED_TASKS_IMMEDIATELY(p);}}}
  job_.phase=DeleteOld;}
 void deleteOld(){int old=job_.old;
  if(tw::exists(old)){sn::SET_ENTITY_AS_MISSION_ENTITY(old,true,true);
   if(sn::DOES_ENTITY_BELONG_TO_THIS_SCRIPT(old,false)){sn::SET_ENTITY_COORDS(old,tw::v3(32.2653f,7683.5249f,.5696f),false,false,false,true);sn::DELETE_VEHICLE(&old);}
   else{sn::RESET_ENTITY_ALPHA(old);sn::SET_ENTITY_COLLISION(old,true,true);tw::notify("The previous vehicle is owned by the game and was kept.");}}
  job_.phase=Finish;}
 void finish(){
  const int v=job_.vehicle;
  if(tw::exists(v)){
   if(job_.radio==255)sn::SET_VEH_RADIO_STATION(v,"OFF");
   else if(job_.radio<=sn::GET_NUM_UNLOCKED_RADIO_STATIONS())sn::SET_VEH_RADIO_STATION(v,sn::GET_RADIO_STATION_NAME(job_.radio));
   sn::SET_VEHICLE_DOORS_SHUT(v,true);sn::SET_VEHICLE_IS_STOLEN(v,false);
  }
  sn::SET_MODEL_AS_NO_LONGER_NEEDED(static_cast<int>(job_.hash));
  if(job_.recipe>=0){job_.op=1;job_.phase=FunnyOp;job_.started=tw::timer();job_.attempts=0;return;}
  if(job_.settings&&tw::exists(v)){
   spawnMaxUpgrades(v,upgraded_,invincible_,plateType_,plateText_==0?"MENYOO":"",neon_,neonRgb_[0],neonRgb_[1],neonRgb_[2],primary_,secondary_);
   if(blip_){const int b=sn::ADD_BLIP_FOR_ENTITY(v);sn::SET_BLIP_SPRITE(b,randomSprite());tw::notify("Added a blip.");}
   if(nearest_)for(int i=1;i<40;++i){tw::V3 out=tw::v3(0,0,0);
    if(!sn::GET_NTH_CLOSEST_VEHICLE_NODE(job_.node,i,&out,1,3.f,0.f))continue;
    if(!sn::IS_POINT_OBSCURED_BY_A_MISSION_ENTITY(out,tw::v3(5.f,5.f,5.f),0)){sn::SET_ENTITY_COORDS(v,out,false,false,false,false);break;}}
   if(!persistent_){int h=v;sn::SET_VEHICLE_AS_NO_LONGER_NEEDED(&h);}
  }
  job_=Job();
 }
 // FunnyVehicles.cpp AttachObjectToVehicle / AttachVehicleToVehicle / AttachPedToVehicle and the inline colour/door/alpha
 // statements, one recipe op per UPDATE (each attached model waits for streaming like Menyoo's Model::Load).
 void hideBase(int base,unsigned model){
  if(sn::IS_THIS_MODEL_A_BICYCLE(static_cast<int>(model))||sn::IS_THIS_MODEL_A_BIKE(static_cast<int>(model))){sn::SET_ENTITY_ALPHA(base,0,false);return;}
  if(!sn::IS_ENTITY_VISIBLE(base))return;sn::SET_ENTITY_VISIBLE(base,false,false);
  const int maximum=sn::GET_VEHICLE_MAX_NUMBER_OF_PASSENGERS(base)-2;
  for(int i=-1;i<=maximum;++i){if(sn::IS_VEHICLE_SEAT_FREE(base,i,false))continue;const int p=sn::GET_PED_IN_VEHICLE_SEAT(base,i,false);if(tw::exists(p))sn::SET_ENTITY_VISIBLE(p,true,false);}
 }
 int boneOf(int base,const char*bone){if(!bone||!*bone)return -1;if(!strcmp(bone,"#0"))return 0;return sn::GET_ENTITY_BONE_INDEX_BY_NAME(base,bone);}
 bool loaded(uint32_t model){
  if(!sn::IS_MODEL_VALID(static_cast<int>(model)))return false;sn::REQUEST_MODEL(static_cast<int>(model));
  return sn::HAS_MODEL_LOADED(static_cast<int>(model));
 }
 void funnyOp(){
  const int base=job_.vehicle;const funny::Recipe&recipe=funny::RECIPES[job_.recipe];
  if(!tw::exists(base)||job_.op>=recipe.count){job_=Job();return;}
  const funny::Op&o=funny::OPS[recipe.first+job_.op];const tw::V3 pos=tw::v3(o.p[0],o.p[1],o.p[2]),rot=tw::v3(o.p[3],o.p[4],o.p[5]);
  if((o.kind==funny::Object||o.kind==funny::Vehicle||o.kind==funny::Ped)&&!loaded(o.hash)){
   if(tw::timer()-job_.started<3000)return;sn::SET_MODEL_AS_NO_LONGER_NEEDED(static_cast<int>(o.hash));++job_.op;job_.started=tw::timer();return;}
  int a=0,b=0;
  switch(o.kind){
  case funny::Object:{
   const int object=sn::CREATE_OBJECT(static_cast<int>(o.hash),tw::v3(0,0,0),false,false,(o.flags&funny::Dynamic)!=0);
   if(object){sn::SET_ENTITY_LOD_DIST(object,1000000);if(o.flags&funny::Invisible)hideBase(base,o.hash);
    tw::V3 at=pos;if(o.flags&funny::StackZ){tw::V3 vl,vh,bl,bh;tw::dims(tw::model(base),vl,vh);tw::dims(o.hash,bl,bh);at.z=vh.z+bl.z;}
    sn::ATTACH_ENTITY_TO_ENTITY(object,base,boneOf(base,o.bone),at,rot,false,false,false,false,2,true,false);
    sn::SET_ENTITY_LIGHTS(object,false);if(o.flags&funny::Collision)sn::SET_ENTITY_COLLISION(object,true,false);
    if(o.flags&funny::Release){int h=object;sn::SET_OBJECT_AS_NO_LONGER_NEEDED(&h);}}
   sn::SET_MODEL_AS_NO_LONGER_NEEDED(static_cast<int>(o.hash));break;}
  case funny::Vehicle:{
   if(!guard(GuardModel,o.hash)){sn::SET_MODEL_AS_NO_LONGER_NEEDED(static_cast<int>(o.hash));break;}
   const int veh=sn::CREATE_VEHICLE(static_cast<int>(o.hash),tw::v3(0,0,0),0.f,false,false,false);
   if(veh&&!guard(GuardInstance,static_cast<uint32_t>(veh))){deleteOwn(veh);sn::SET_MODEL_AS_NO_LONGER_NEEDED(static_cast<int>(o.hash));break;}
   if(veh){sn::SET_ENTITY_LOD_DIST(veh,1000000);if(o.flags&funny::Invisible)hideBase(base,o.hash);
    sn::ATTACH_ENTITY_TO_ENTITY(veh,base,boneOf(base,o.bone),pos,rot,false,false,false,false,2,true,false);sn::SET_ENTITY_LIGHTS(veh,false);
    sn::GET_VEHICLE_COLOURS(veh,&a,&b);sn::SET_VEHICLE_COLOURS(veh,o.a,b);sn::GET_VEHICLE_COLOURS(veh,&a,&b);sn::SET_VEHICLE_COLOURS(veh,a,o.b);
    if(o.flags&funny::Collision)sn::SET_ENTITY_COLLISION(veh,true,false);int h=veh;sn::SET_VEHICLE_AS_NO_LONGER_NEEDED(&h);}
   sn::SET_MODEL_AS_NO_LONGER_NEEDED(static_cast<int>(o.hash));break;}
  case funny::Ped:{
   if(o.flags&funny::Invisible)hideBase(base,tw::model(base));
   const int ped=sn::CREATE_PED(26,static_cast<int>(o.hash),tw::v3(0,0,0),sn::GET_ENTITY_HEADING(base),false,false);
   if(ped){sn::SET_ENTITY_LOD_DIST(ped,696969);
    sn::ATTACH_ENTITY_TO_ENTITY(ped,base,-1,tw::v3(0,0,0),tw::v3(0,0,180.f),false,false,false,false,2,true,false);sn::DETACH_ENTITY(ped,true,true);
    sn::ATTACH_ENTITY_TO_ENTITY(ped,base,-1,pos,rot,false,false,false,false,2,true,false);sn::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(ped,true);
    sn::SET_ENTITY_INVINCIBLE(ped,true);sn::SET_PED_DIES_IN_WATER(ped,false);sn::SET_ENTITY_PROOFS(ped,true,true,true,true,true,true,true,true);
    if(o.flags&funny::Piggyback){sn::ATTACH_ENTITY_TO_ENTITY(ped,base,-1,tw::v3(0,-.3f,0),tw::v3(0,0,0),false,false,false,false,2,true,false);
     sn::REQUEST_ANIM_DICT("mini@prostitutes@sexnorm_veh");job_.anim=ped;job_.phase=FunnyAnim;job_.started=tw::timer();
     sn::SET_MODEL_AS_NO_LONGER_NEEDED(static_cast<int>(o.hash));return;}
    sn::TASK_STAND_STILL(ped,-1);int h=ped;sn::SET_PED_AS_NO_LONGER_NEEDED(&h);}
   sn::SET_MODEL_AS_NO_LONGER_NEEDED(static_cast<int>(o.hash));break;}
  case funny::Primary:sn::GET_VEHICLE_COLOURS(base,&a,&b);sn::SET_VEHICLE_COLOURS(base,o.a,b);break;
  case funny::Secondary:sn::GET_VEHICLE_COLOURS(base,&a,&b);sn::SET_VEHICLE_COLOURS(base,a,o.a);break;
  case funny::BreakDoors:for(int d=0;d<=6;++d){sn::SET_DOOR_ALLOWED_TO_BE_BROKEN_OFF(base,d,true);sn::SET_VEHICLE_DOOR_BROKEN(base,d,true);}break;
  case funny::Upgrade:spawnMaxUpgrades(base,true,false,5,"",false,0,0,0,-3,-3);break;
  case funny::Visible:sn::SET_ENTITY_VISIBLE(base,o.a!=0,false);break;
  case funny::Alpha:sn::SET_ENTITY_ALPHA(base,o.a,false);break;
  }
  ++job_.op;job_.started=tw::timer();
 }
 void funnyAnim(){const int ped=job_.anim;
  if(!sn::HAS_ANIM_DICT_LOADED("mini@prostitutes@sexnorm_veh")&&tw::timer()-job_.started<1000)return;
  if(tw::exists(ped)){sn::TASK_PLAY_ANIM(ped,"mini@prostitutes@sexnorm_veh","bj_loop_male",8.f,0.f,-1,9,0.f,false,false,false);int h=ped;sn::SET_PED_AS_NO_LONGER_NEEDED(&h);}
  job_.anim=0;++job_.op;job_.phase=FunnyOp;job_.started=tw::timer();
 }
 bool start(uint32_t hash,bool settings,int recipe){
  if(job_.phase!=Idle){tw::notify("A vehicle is already being created.");return false;}
  if(!sn::IS_MODEL_VALID(static_cast<int>(hash))||!sn::IS_MODEL_A_VEHICLE(static_cast<int>(hash))){tw::notify("Invalid vehicle model.");return false;}
  job_=Job();job_.phase=Load;job_.hash=hash;job_.settings=settings;job_.recipe=recipe;job_.started=tw::timer();
  job_.deleteOld=recipe<0&&deleteOld_;job_.warp=recipe<0?autoSit_:true;job_.radio=sn::GET_PLAYER_RADIO_STATION_INDEX();
  job_.node=tw::position(tw::ped());sn::REQUEST_MODEL(static_cast<int>(hash));sn::REQUEST_NAMED_PTFX_ASSET("proj_xmas_firework");return true;
 }
 static bool toggle(Transport&m,const char*label,bool&value,const char*help){const bool c=m.add(label,ToggleRow,value?"ON":"OFF");if(help)m.description(help);
  if(c&&m.event()==Accept){value=!value;return true;}return false;}
public:
 VehicleSpawner():catalogue_(NULL),customs_(NULL){memset(&ids_,0,sizeof(ids_));resetSettings();job_=Job();}
 void resetSettings(){deleteOld_=blip_=nearest_=upgraded_=invincible_=persistent_=neon_=false;autoSit_=previews_=true;
  primary_=secondary_=-3;neonRgb_[0]=0;neonRgb_[1]=255;neonRgb_[2]=0;plateType_=5;plateText_=0;view_=0;dlc_=0;viewsBuilt_=false;names_.clear();}
 void bind(const SpawnerPageIds&ids,VehicleActions&catalogue,VehicleCustoms&customs){
  ids_=ids;catalogue_=&catalogue;customs_=&customs;customs.bindSpawn(&primary_,&secondary_,neonRgb_);job_=Job();viewsBuilt_=false;names_.clear();}
 void refresh(){viewsBuilt_=false;names_.clear();}
 bool busy()const{return job_.phase!=Idle;}
 bool page(Transport&m){
  m.setTitle("Vehicles");
  m.add("Spawn Settings",ActionRow,"",ids_.settings);
  m.add("Funny Vehicles (Old)",ActionRow,"",ids_.funny);m.description("Pre-built joke vehicles made from attached props.");
  buildViews();
  if(!viewsBuilt_){m.add("Unavailable",ActionRow,"");m.rows.back().enabled=false;m.description("Native vehicle catalogue unavailable.");return true;}
  for(unsigned i=0;i<24;++i){
   if(i==0)m.add("---Cars---",BreakRow,"");else if(i==11)m.add("---Industrial---",BreakRow,"");else if(i==17)m.add("---Others---",BreakRow,"");
   const View&v=view(i);if(views_[v.group].empty())continue; // AddVehicleCategoryOption skips empty classes
   if(m.add(v.name,ActionRow,"",ids_.category)&&m.event()==Accept)view_=v.group;}
  m.add("DLC Vehicles",ActionRow,"",ids_.dlc);m.description("Vehicles grouped by the update that added them.");
  if(m.add("Random Vehicle",ActionRow,"")&&m.event()==Accept){unsigned total=0;for(unsigned g=0;g<24;++g)total+=static_cast<unsigned>(views_[g].size());
   if(total){unsigned pick=static_cast<unsigned>(sn::GET_RANDOM_INT_IN_RANGE(0,static_cast<int>(total)));
    for(unsigned g=0;g<24;++g){if(pick<views_[g].size()){const CatalogueEntry*e=viewEntry(g,pick);if(e)start(e->hash,true,-1);break;}pick-=static_cast<unsigned>(views_[g].size());}}}
  m.description("Spawns a random vehicle.");
  return true;
 }
 bool categoryPage(Transport&m){
  buildViews();const unsigned g=static_cast<unsigned>(view_);
  const char*title="Vehicles";for(unsigned i=0;i<24;++i)if(view(i).group==view_)title=view(i).name;m.setTitle(title);
  for(unsigned i=0;g<24&&i<views_[g].size();++i){const CatalogueEntry*e=viewEntry(g,i);if(!e)continue;
   if(m.add(displayName(e->hash,e->name),ActionRow,"")&&m.event()==Accept)start(e->hash,true,-1);
   m.icons(sn::IS_THIS_MODEL_A_BIKE(static_cast<int>(e->hash))||sn::IS_THIS_MODEL_A_BICYCLE(static_cast<int>(e->hash)),BikeIcon,CarIcon);
   previewRow(m,e->hash);}
  return true;
 }
 bool dlcPage(Transport&m){
  m.setTitle("DLC Vehicles");
  for(unsigned i=0;i<sizeof(spawner::DLC_CATEGORIES)/sizeof(spawner::DLC_CATEGORIES[0]);++i)
   if(m.add(spawner::DLC_CATEGORIES[i].name,ActionRow,"",ids_.dlcItems)&&m.event()==Accept)dlc_=static_cast<int>(i);
  return true;
 }
 bool dlcItemsPage(Transport&m){
  const unsigned n=sizeof(spawner::DLC_CATEGORIES)/sizeof(spawner::DLC_CATEGORIES[0]);
  if(dlc_<0||static_cast<unsigned>(dlc_)>=n){m.setTitle("DLC Vehicles");m.add("Invalid Category",ActionRow,"");m.rows.back().enabled=false;return true;}
  const spawner::DlcCategory&c=spawner::DLC_CATEGORIES[dlc_];m.setTitle(c.name);
  for(unsigned i=0;i<c.count;++i){const spawner::DlcVehicle&v=spawner::DLC_VEHICLES[c.first+i];
   const bool installed=sn::IS_MODEL_VALID(static_cast<int>(v.hash))&&sn::IS_MODEL_A_VEHICLE(static_cast<int>(v.hash));
   if(m.add(v.caption,ActionRow,"")&&m.event()==Accept&&installed)start(v.hash,true,-1);
   if(!installed){m.rows.back().enabled=false;m.description("This model is not installed in this game version.");}
   previewRow(m,v.hash);}
  return true;
 }
 bool settingsPage(Transport&m){
  m.setTitle("Spawn Settings");
  toggle(m,"Delete Old Vehicle",deleteOld_,"Deletes your previous vehicle when you spawn a new one.");
  toggle(m,"Auto-Sit In Vehicle",autoSit_,"Puts you in the driver seat after spawning.");
  toggle(m,"Add Blip For Spawned Vehicles",blip_,"Adds a map blip on each spawned vehicle.");
  toggle(m,"Spawn At Nearest Node",nearest_,"Spawns on the nearest road instead of in front of you.");
  toggle(m,"Spawn Pre-Upgraded",upgraded_,"Applies upgrades to new vehicles.");
  toggle(m,"Spawn Invincible",invincible_,NULL);
  toggle(m,"Spawn Persistent",persistent_,"Spawned vehicles won't despawn.");
  if(m.add("Primary Paint",ActionRow,"",ids_.paints2)&&m.event()==Accept&&customs_)customs_->editSpawnPaint(true);
  if(m.add("Secondary Paint",ActionRow,"",ids_.paints2)&&m.event()==Accept&&customs_)customs_->editSpawnPaint(false);
  m.add("---Neons---",BreakRow,"");
  toggle(m,"Toggle",neon_,"Spawned vehicles get neon lights in the colour below.");
  char rgb[32];sprintf_s(rgb,"%d, %d, %d",neonRgb_[0],neonRgb_[1],neonRgb_[2]);
  if(m.add("RGB Colour",ActionRow,rgb,ids_.rgb)&&m.event()==Accept&&customs_)customs_->editSpawnNeon();
  m.add("---Plate---",BreakRow,"");
  const std::string type=tw::text(plateTypeLabel(plateType_),plateTypeLabel(plateType_));
  if(m.add("Type",ChoiceRow,type)&&(m.event()==Left||m.event()==Right))plateType_=(plateType_+(m.event()==Right?1:12))%13;
  if(m.add("Text",ChoiceRow,plateText_==0?"MENYOO":"Random")&&(m.event()==Left||m.event()==Right))plateText_=1-plateText_;
  m.description("MENYOO or the game's random plate text. Typing custom text is not available in VR.");
  m.add("---Previews---",BreakRow,"");
  toggle(m,"Enable Previews",previews_,"Shows a vehicle image while browsing.");
  if(m.add("Reload Previews",ActionRow,"")&&m.event()==Accept){names_.clear();tw::notify("Previews reloaded.");}
  m.description("Reloads the preview image files.");
  return true;
 }
 bool funnyPage(Transport&m){
  m.setTitle("Badly Constructed Vehicles");
  for(unsigned i=0;i<sizeof(funny::RECIPES)/sizeof(funny::RECIPES[0]);++i){
   const funny::Recipe&r=funny::RECIPES[i];if(m.add(r.name,ActionRow,"")&&m.event()==Accept)start(funny::OPS[r.first].hash,false,static_cast<int>(i));}
  return true;
 }
 // One job step per UPDATE; never blocks the fiber.
 void tick(){
  switch(job_.phase){
  case Load:if(sn::HAS_MODEL_LOADED(static_cast<int>(job_.hash)))job_.phase=Create;else if(tw::timer()-job_.started>3000)fail("Vehicle model did not load in 3 s.");
   else sn::REQUEST_MODEL(static_cast<int>(job_.hash));break;
  case Create:create();break;
  case LeaveOld:leaveOld();break;
  case DeleteOld:deleteOld();break;
  case Finish:finish();break;
  case FunnyOp:funnyOp();break;
  case FunnyAnim:funnyAnim();break;
  default:break;}
 }
 void suspend(){if(job_.hash)sn::SET_MODEL_AS_NO_LONGER_NEEDED(static_cast<int>(job_.hash));job_=Job();}
};
}
