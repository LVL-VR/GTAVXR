#pragma once
// retained Menyoo Customs (rebranded Los Santos Customs; curtis VehicleModShop.cpp). Mod slots, Benny's, paints
// (dynamic Classic/Metallic/Pearl/Matte/Metal/Chrome/Chameleon lists read from the game's mod colour tables), pinned
// named tables (vehicle_customs_data.h), wheels/tyres, neons + animations, doors, windows, lights, extras, removable
// components, crew emblem, engine sound, power/torque/top speed multipliers, liveries, plates, upgrades.
// REMOVE: plate text entry (CMOD_MOD_18_D InputBox), hex/number text input, engine-sound Input Model.
// Text entries are replaced by steppers; apply-on-press (Menyoo non-LSC navigation mode).
#include "trainer_world.h"
#include "vehicle_customs_data.h"
#include "menu_transport.h"
#include <map>
#include <stdio.h>
#include <stdlib.h>
namespace menyoo_vr {
struct CustomsPageIds {int modshop,bennys,catall,paints,paints2,chrome,classic,matte,metallic,metal,chameleon,pearl,util,worn,shared,rgb,
 wheels,wheels2,wheels3,neons,doors,windows,emblem,removable,lights,engineSound,engineSoundItems;};
class VehicleCustoms {
 struct Named {std::string name;int paint,pearl;};
 CustomsPageIds ids_;int target_,modSlot_,paintPart_,rgbMode_,wheelType_,wheelRear_,windowMode_,doorAction_,soundGroup_;
 std::vector<Named>paints_[7];bool paintsLoaded_;
 int neonFade_,neonFlash_,neonDelay_,neonR_,neonG_,neonB_;bool neonRgb_,neonState_[4],flashOn_,fwk_[4];int spin_,spinBack_,flashTick_,spinTick_;
 bool leftInd_,rightInd_,hazard_;std::map<int,float>rpm_,torque_,maxSpeed_,lights_;bool lowered_;
 int cacheVehicle_;std::vector<int>brokenDoors_,burstTyres_;
 // Spawn Settings targets (Menyoo s_selectedPaintPart 10/11 and the spawn neon RGB): edit settings, not a vehicle.
 int*spawnPaint_[2];int*spawnNeon_;
 bool spawnPaints()const{return paintPart_>=10&&spawnPaint_[0]&&spawnPaint_[1];}
 bool spawnRgb()const{return rgbMode_==9&&spawnNeon_;}
 static const char*wheelTypeName(int i){static const char*n[13]={"Sport","Muscle","Lowrider","SUV","Offroad","Tuner","Bike","High-End",
  "Benny's Originals","Benny's Bespoke","Open Wheel","Street","Track"};return i>=0&&i<13?n[i]:"";}
 bool valid()const{return tw::exists(target_)&&sn::IS_ENTITY_A_VEHICLE(target_);}
 static bool bikeModel(unsigned m){return sn::IS_THIS_MODEL_A_BIKE(static_cast<int>(m))||sn::IS_THIS_MODEL_A_BICYCLE(static_cast<int>(m))||sn::IS_THIS_MODEL_A_QUADBIKE(static_cast<int>(m));}
 static std::string slotName(int v,int slot){
  std::string name;if(slot<0||slot>=49)return "CMM_MOD INVALID";
  if(!sn::HAS_ADDITIONAL_TEXT_LOADED(19))sn::REQUEST_ADDITIONAL_TEXT("mod_mnu",19);
  const char*s=sn::GET_MOD_SLOT_NAME(v,slot);name=(s&&strlen(s)>=2)?s:customs::MOD_SLOT_NAMES[slot];
  return tw::text(name.c_str(),name.c_str());
 }
 static std::string modLabel(int v,int slot,int value){
  if(value<0){if(slot==1)return tw::text("CMOD_BUM_0","Stock Front Bumper");if(slot==2)return tw::text("CMOD_BUM_3","Stock Rear Bumper");return "Stock "+slotName(v,slot);}
  static const char*armour[5]={"CMOD_ARM_1","CMOD_ARM_2","CMOD_ARM_3","CMOD_ARM_4","CMOD_ARM_5"};
  static const char*suspension[5]={"Lowered ","Street ","Sport ","Competition ","Race "};
  static const char*brake[4]={"Street ","Sport ","Race ","Super "};char n[32];
  switch(slot){
  case 11:sprintf_s(n,"EMS Upgrade %d",value+1);return n;
  case 12:case 13:if(value>=4){sprintf_s(n,"%d",value);return slotName(v,slot)+n;}return brake[value]+slotName(v,slot);
  case 14:if(value>=58||!customs::HORN_NAMES[value][0]){sprintf_s(n,"%d",value);return slotName(v,slot)+n;}return tw::text(customs::HORN_NAMES[value],customs::HORN_NAMES[value]);
  case 15:if(value>=5){sprintf_s(n,"%d",value);return slotName(v,slot)+n;}return suspension[value]+slotName(v,slot);
  case 16:if(value>=5){sprintf_s(n,"Armour Upgrade %d%%",(value+1)*20);return n;}return tw::text(armour[value],armour[value]);
  }
  const char*label=sn::GET_MOD_TEXT_LABEL(v,slot,value);
  if(label&&strlen(label)>3)return tw::text(label,label);
  sprintf_s(n," %d",value+1);return slotName(v,slot)+n;
 }
 int part(int p)const{int a=0,b=0;
  switch(p){case 1:sn::GET_VEHICLE_COLOURS(target_,&a,&b);return a;case 2:sn::GET_VEHICLE_COLOURS(target_,&a,&b);return b;
   case 3:sn::GET_VEHICLE_EXTRA_COLOURS(target_,&a,&b);return a;case 4:sn::GET_VEHICLE_EXTRA_COLOURS(target_,&a,&b);return b;
   case 5:sn::GET_VEHICLE_EXTRA_COLOUR_5(target_,&a);return a;case 6:sn::GET_VEHICLE_EXTRA_COLOUR_6(target_,&a);return a;
   case 10:case 11:return spawnPaint_[p-10]?*spawnPaint_[p-10]:-3;}return 0;}
 void paint(int p,int colour,int pearl){
  int a=0,b=0;
  if(p==10||p==11){if(spawnPaint_[p-10])*spawnPaint_[p-10]=colour;return;} // Menyoo paintCarUsing_index 10/11
  switch(p){
  case 1:sn::CLEAR_VEHICLE_CUSTOM_PRIMARY_COLOUR(target_);sn::GET_VEHICLE_COLOURS(target_,&a,&b);sn::SET_VEHICLE_COLOURS(target_,colour,b);
   if(pearl!=-1){sn::GET_VEHICLE_EXTRA_COLOURS(target_,&a,&b);sn::SET_VEHICLE_EXTRA_COLOURS(target_,pearl,b);}break;
  case 2:sn::CLEAR_VEHICLE_CUSTOM_SECONDARY_COLOUR(target_);sn::GET_VEHICLE_COLOURS(target_,&a,&b);sn::SET_VEHICLE_COLOURS(target_,a,colour);break;
  case 3:sn::GET_VEHICLE_EXTRA_COLOURS(target_,&a,&b);sn::SET_VEHICLE_EXTRA_COLOURS(target_,colour,b);break;
  case 4:sn::GET_VEHICLE_EXTRA_COLOURS(target_,&a,&b);sn::SET_VEHICLE_EXTRA_COLOURS(target_,a,colour);break;
  case 5:sn::SET_VEHICLE_EXTRA_COLOUR_5(target_,colour);break;
  case 6:sn::SET_VEHICLE_EXTRA_COLOUR_6(target_,colour);break;}
 }
 // Menyoo PopulateAllPaintIDs, on the target vehicle with its paint captured and restored (no dummy vehicle spawn).
 void loadPaints(){
  if(paintsLoaded_)return;const int saved=target_;if(!valid())target_=tw::vehicle();if(!valid()){target_=saved;return;}
  int p=0,s=0,pearl=0,wheel=0,r=0,g=0,b=0,sr=0,sg=0,sb=0;
  const bool customP=sn::GET_IS_VEHICLE_PRIMARY_COLOUR_CUSTOM(target_),customS=sn::GET_IS_VEHICLE_SECONDARY_COLOUR_CUSTOM(target_);
  sn::GET_VEHICLE_COLOURS(target_,&p,&s);sn::GET_VEHICLE_EXTRA_COLOURS(target_,&pearl,&wheel);
  if(customP)sn::GET_VEHICLE_CUSTOM_PRIMARY_COLOUR(target_,&r,&g,&b);if(customS)sn::GET_VEHICLE_CUSTOM_SECONDARY_COLOUR(target_,&sr,&sg,&sb);
  sn::SET_VEHICLE_MOD_KIT(target_,0);
  for(int type=0;type<7;++type){const int count=sn::GET_NUM_MOD_COLORS(type,false);paints_[type].clear();
   for(int i=0;i<count&&i<512;++i){int colour=0,second=0,pr=0,wh=0;sn::SET_VEHICLE_MOD_COLOR_1(target_,type,i,0);
    sn::GET_VEHICLE_EXTRA_COLOURS(target_,&pr,&wh);sn::GET_VEHICLE_COLOURS(target_,&colour,&second);
    const char*label=sn::GET_VEHICLE_MOD_COLOR_1_NAME(target_,false);char fallback[32];sprintf_s(fallback,"Extra Colour %d",i);
    Named n;n.name=label&&*label?tw::text(label,fallback):std::string(fallback);n.paint=type==2?-1:colour;n.pearl=type==2?colour:pr;paints_[type].push_back(n);}}
  sn::SET_VEHICLE_COLOURS(target_,p,s);sn::SET_VEHICLE_EXTRA_COLOURS(target_,pearl,wheel);
  if(customP)sn::SET_VEHICLE_CUSTOM_PRIMARY_COLOUR(target_,r,g,b);if(customS)sn::SET_VEHICLE_CUSTOM_SECONDARY_COLOUR(target_,sr,sg,sb);
  paintsLoaded_=true;target_=saved;
 }
 bool tick(Transport&m,const std::string&label,bool state,Icon icon=CarIcon){const bool chosen=m.add(label,ActionRow,"");m.icons(state,icon,NoIcon);return chosen&&m.event()==Accept;}
 bool toggle(Transport&m,const std::string&label,bool state,const char*help=NULL){const bool chosen=m.add(label,ToggleRow,state?"ON":"OFF");if(help)m.description(help);return chosen&&m.event()==Accept;}
 int stepper(Transport&m,const std::string&label,const char*value,const char*help=NULL){const bool chosen=m.add(label,NumberRow,value);if(help)m.description(help);
  return chosen?(m.event()==Right?1:(m.event()==Left?-1:0)):0;}
 int choice(Transport&m,const std::string&label,const std::string&value,const char*help=NULL){const bool chosen=m.add(label,ChoiceRow,value);if(help)m.description(help);
  return chosen?(m.event()==Right?1:(m.event()==Left?-1:0)):0;}
 void paintList(Transport&m,const char*title,const std::vector<Named>&list,bool pearlList){
  m.setTitle(title);const int current=part(paintPart_);const unsigned model=valid()?tw::model(target_):0u;
  if(list.empty()){m.add("Unavailable",ActionRow,"");m.rows.back().enabled=false;m.description("Enter any vehicle once to load the game's paint list.");}
  for(size_t i=0;i<list.size();++i){const int value=pearlList?list[i].pearl:list[i].paint;
   if(tick(m,list[i].name,current==value,bikeModel(model)?BikeIcon:CarIcon))paint(paintPart_,value,pearlList?list[i].pearl:list[i].pearl);}
 }
 void fixedList(Transport&m,const char*title,const customs::Paint*list,unsigned count){
  m.setTitle(title);const int current=part(paintPart_);
  for(unsigned i=0;i<count;++i)if(tick(m,list[i].name,current==list[i].paint))paint(paintPart_,list[i].paint,list[i].pearl);
 }
 void indexSlider(Transport&m){int total=0;for(int i=0;i<5;++i)total+=sn::GET_NUM_MOD_COLORS(i,true);const int maximum=160+(total-232);
  int index=part(paintPart_);char t[16];sprintf_s(t,"%d",index);const int d=stepper(m,"Paint Index",t);
  if(d){index+=d;if(index>maximum)index=0;if(index<0)index=maximum;paint(paintPart_,index,-1);}}
 void neonColour(int r,int g,int b){sn::SET_VEHICLE_NEON_COLOUR(target_,r,g,b);}
 void setRgb(int r,int g,int b){
  if(spawnRgb()){spawnNeon_[0]=r;spawnNeon_[1]=g;spawnNeon_[2]=b;return;}
  if(sn::GET_VEHICLE_MOD_KIT(target_)!=0)sn::SET_VEHICLE_MOD_KIT(target_,0);
  switch(rgbMode_){case 0:sn::SET_VEHICLE_CUSTOM_PRIMARY_COLOUR(target_,r,g,b);break;case 1:sn::SET_VEHICLE_CUSTOM_SECONDARY_COLOUR(target_,r,g,b);break;
   case 2:neonR_=r;neonG_=g;neonB_=b;neonColour(r,g,b);break;case 4:sn::TOGGLE_VEHICLE_MOD(target_,20,true);sn::SET_VEHICLE_TYRE_SMOKE_COLOR(target_,r,g,b);break;}
 }
 bool doorOpen(int door)const{return sn::GET_VEHICLE_DOOR_ANGLE_RATIO(target_,door)>0.f;}
 std::vector<int>doors()const{std::vector<int>d;static const char*bones[7]={"door_dside_f","door_pside_f","door_dside_r","door_pside_r","bonnet","boot","boot_2"};
  for(int i=0;i<7;++i)if(tw::bone(target_,bones[i]))d.push_back(i);return d;}
 void refreshCache(){if(cacheVehicle_==target_)return;cacheVehicle_=target_;brokenDoors_.clear();burstTyres_.clear();
  std::vector<int>d=doors();for(size_t i=0;i<d.size();++i)if(sn::IS_VEHICLE_DOOR_DAMAGED(target_,d[i]))brokenDoors_.push_back(d[i]);
  static const char*tyreBones[6]={"wheel_lf","wheel_rf","wheel_lm1","wheel_rm1","wheel_lr","wheel_rr"};
  for(int i=0;i<6;++i)if(tw::bone(target_,tyreBones[i])&&sn::IS_VEHICLE_TYRE_BURST(target_,i,false))burstTyres_.push_back(i);}
 static bool has(const std::vector<int>&v,int x){for(size_t i=0;i<v.size();++i)if(v[i]==x)return true;return false;}
 static void erase(std::vector<int>&v,int x){for(size_t i=0;i<v.size();++i)if(v[i]==x){v.erase(v.begin()+static_cast<long>(i));return;}}
public:
 VehicleCustoms():target_(0),modSlot_(0),paintPart_(1),rgbMode_(0),wheelType_(0),wheelRear_(0),windowMode_(0),doorAction_(0),soundGroup_(0),paintsLoaded_(false),
  neonFade_(0),neonFlash_(0),neonDelay_(1000),neonR_(255),neonG_(255),neonB_(255),neonRgb_(false),flashOn_(false),spin_(0),spinBack_(0),flashTick_(0),spinTick_(0),
  leftInd_(false),rightInd_(false),hazard_(false),lowered_(false),cacheVehicle_(0),spawnNeon_(NULL){spawnPaint_[0]=spawnPaint_[1]=NULL;memset(&ids_,0,sizeof(ids_));memset(neonState_,0,sizeof(neonState_));memset(fwk_,0,sizeof(fwk_));}
 void bind(const CustomsPageIds&ids){ids_=ids;}
 // Spawn Settings: Primary/Secondary Paint (-3 = None) and the neon RGB colour edit these settings (Menyoo parts 10/11/9).
 void bindSpawn(int*primary,int*secondary,int*neonRgb){spawnPaint_[0]=primary;spawnPaint_[1]=secondary;spawnNeon_=neonRgb;}
 void editSpawnPaint(bool primary){paintPart_=primary?10:11;}
 void editSpawnNeon(){rgbMode_=9;}
 void editVehicle(){if(paintPart_>=10)paintPart_=1;if(rgbMode_==9)rgbMode_=0;}
 void target(int vehicle){if(vehicle!=target_){cacheVehicle_=0;memset(neonState_,0,sizeof(neonState_));neonFade_=neonFlash_=0;neonRgb_=false;}target_=vehicle;}
 bool hasTarget()const{return valid();}
 // ---------------------------------------------------------------- main page
 bool page(Transport&m){
  if(!valid()){m.setTitle("Los Santos Customs");m.add("You are not in a vehicle.",ActionRow,"");m.rows.back().enabled=false;return true;}
  const int v=target_;const unsigned model=tw::model(v);const bool bike=bikeModel(model);char t[64];
  if(sn::GET_VEHICLE_MOD_KIT(v)!=0)sn::SET_VEHICLE_MOD_KIT(v,0);
  m.setTitle("Menyoo Customs");
  m.add(tw::text("S_MO_09","Benny's Lowrider Mods"),ActionRow,"",ids_.bennys);
  if(sn::GET_VEHICLE_MOD_KIT(v)!=65535&&!sn::IS_THIS_MODEL_A_BICYCLE(static_cast<int>(model)))m.add(tw::text("CMOD_MOD_WHEM","Wheels"),ActionRow,"",ids_.wheels);
  for(int i=0;i<=24;++i){if(i>=17&&i<=24)continue;if(sn::GET_NUM_VEHICLE_MODS(v,i)>0&&m.add(slotName(v,i),ActionRow,"",ids_.catall)&&m.event()==Accept)modSlot_=i;}
  m.add("Paints",ActionRow,"",ids_.paints);
  if(!bike){m.add(tw::text("PIM_PVEO_004","Neons Lights"),ActionRow,"",ids_.neons);m.add(tw::text("CMM_MOD_S6","Doors"),ActionRow,"",ids_.doors);
   m.add(tw::text("CMOD_MOD_WIN","Windows"),ActionRow,"",ids_.windows);}
  m.add(tw::text("CMOD_COL0_3","Emblem"),ActionRow,"",ids_.emblem);
  m.add("Remove Vehicle Components and Extras",ActionRow,"",ids_.removable);m.description("Break or remove doors, wheels and windows, and toggle extras.");
  const int liveries=sn::GET_VEHICLE_LIVERY_COUNT(v);
  if(liveries>0){int l=sn::GET_VEHICLE_LIVERY(v)+1;sprintf_s(t,"%d",l);const int d=stepper(m,tw::text("CMOD_COL0_4","Livery"),t);
   if(d){l+=d;if(l>liveries)l=liveries;if(l<1)l=1;sn::SET_VEHICLE_LIVERY(v,l-1);}}
  const int roof=sn::GET_VEHICLE_LIVERY2_COUNT(v);
  if(roof>0){int l=sn::GET_VEHICLE_LIVERY2(v)+1;sprintf_s(t,"%d",l);const int d=stepper(m,"Roof Livery",t);
   if(d){l+=d;if(l>roof)l=roof;if(l<1)l=1;sn::SET_VEHICLE_LIVERY2(v,l-1);}}
  if(toggle(m,tw::text("CMOD_MOD_TUR","Turbo"),sn::IS_TOGGLE_MOD_ON(v,18)))sn::TOGGLE_VEHICLE_MOD(v,18,!sn::IS_TOGGLE_MOD_ON(v,18));
  if(toggle(m,tw::text("CMOD_LGT_1","Xenon Lights"),sn::IS_TOGGLE_MOD_ON(v,22)))sn::TOGGLE_VEHICLE_MOD(v,22,!sn::IS_TOGGLE_MOD_ON(v,22));
  if(toggle(m,"Lower Suspension",lowered_,"Lowers the ride height (Tuners-style).")){lowered_=!lowered_;sn::SET_REDUCED_SUSPENSION_FORCE(v,lowered_);}
  if(sn::IS_TOGGLE_MOD_ON(v,22)){static const char*hl[13]={"White","Blue","Light Blue","Green","Light Green","Light Yellow","Yellow","Orange","Red","Light Pink","Pink","Purple","Light Purple"};
   int c=sn::GET_VEHICLE_XENON_LIGHT_COLOR_INDEX(v);const int d=choice(m,tw::text("CMOD_LGT_1","Xenon Lights")+" Colour",c==255||c<0||c>12?"Stock":hl[c]);
   if(d>0){c=(c==255||c<0)?0:(c<12?c+1:12);sn::SET_VEHICLE_XENON_LIGHT_COLOR_INDEX(v,c);}
   else if(d<0){c=(c==0)?255:(c!=255&&c>0?c-1:255);sn::SET_VEHICLE_XENON_LIGHT_COLOR_INDEX(v,c);}}
  float hm=lights_.count(v)?lights_[v]:1.f;sprintf_s(t,"%.2f",hm);int d=stepper(m,tw::text("CMOD_MOD_LGT_H","Headlights")+" Intensity",t,"Headlight brightness multiplier.");
  if(d){hm+=.1f*d;lights_[v]=hm;sn::SET_VEHICLE_LIGHT_MULTIPLIER(v,hm);}
  static const char*plates[13]={"CMOD_PLA_0","CMOD_PLA_4","CMOD_PLA_3","CMOD_PLA_1","CMOD_PLA_2","Yankton","CMOD_PLA_6","CMOD_PLA_7","CMOD_PLA_8","CMOD_PLA_9","CMOD_PLA_10","CMOD_PLA_11","CMOD_PLA_12"};
  int plate=sn::GET_VEHICLE_NUMBER_PLATE_TEXT_INDEX(v);if(plate<0)plate=0;
  d=choice(m,tw::text("CMOD_MOD_PLA","Plate")+" "+tw::text("FMMC_MTYPE","Type"),plate<13?tw::text(plates[plate],plates[plate]):std::string("?"));
  if(d){plate+=d;const int n=sn::GET_NUMBER_OF_VEHICLE_NUMBER_PLATES();if(plate>=0&&plate<n)sn::SET_VEHICLE_NUMBER_PLATE_TEXT_INDEX(v,plate);}
  m.add(tw::text("CMM_MOD_G3","Engine")+" "+tw::text("VEUI_AUD_TIT","Sound"),ActionRow,"",ids_.engineSound);
  float rpm=rpm_.count(v)?rpm_[v]:1.f;sprintf_s(t,"%.2f",rpm);d=stepper(m,"Horse Power Multiplier",t,"Multiplies engine power.");
  if(d){rpm+=.1f*d;rpm_[v]=rpm;sn::MODIFY_VEHICLE_TOP_SPEED(v,rpm);}
  float tq=torque_.count(v)?torque_[v]:1.f;sprintf_s(t,"%.2f",tq);d=stepper(m,"Torque Multiplier",t,"Multiplies engine torque (acceleration pull).");
  if(d){tq+=.1f*d;torque_[v]=tq;sn::SET_VEHICLE_CHEAT_POWER_INCREASE(v,tq);}
  float ms=maxSpeed_.count(v)?maxSpeed_[v]:sn::GET_VEHICLE_MODEL_ESTIMATED_MAX_SPEED(static_cast<int>(model));sprintf_s(t,"%.0f",ms*3.6f);
  d=stepper(m,tw::text("FMMC_VEHST_0","Top Speed")+" (Kmph)",t,"Maximum speed limit for this vehicle.");
  if(d){ms+=d/3.6f;maxSpeed_[v]=ms;sn::SET_VEHICLE_MAX_SPEED(v,ms);}
  m.add(tw::text("CMOD_MOD_LGT","Lights"),ActionRow,"",ids_.lights);
  if(tick(m,tw::text("CMM_MOD_G3","Engine"),sn::GET_IS_VEHICLE_ENGINE_RUNNING(v),BoxTickIcon))sn::SET_VEHICLE_ENGINE_ON(v,!sn::GET_IS_VEHICLE_ENGINE_RUNNING(v),true,false);
  if(tw::plane(model)){if(m.add("Plane Aileron On",ActionRow,"")&&m.event()==Accept)sn::DISABLE_PLANE_AILERON(v,false,false);m.description("Enables the plane's aileron flaps.");
   if(m.add("Plane Aileron Off",ActionRow,"")&&m.event()==Accept)sn::DISABLE_PLANE_AILERON(v,false,true);m.description("Disables the plane's aileron flaps.");}
  if(tw::bone(v,"siren1")&&tick(m,"Sirens",sn::IS_VEHICLE_SIREN_ON(v),BoxTickIcon))sn::SET_VEHICLE_SIREN(v,!sn::IS_VEHICLE_SIREN_ON(v));
  if(m.add("Random Upgrades",ActionRow,"")&&m.event()==Accept){upgrades(v,true);sn::SET_VEHICLE_TYRES_CAN_BURST(v,false);}
  m.description("Applies random mods to every slot and makes the tyres bulletproof.");
  if(m.add("Return to Stock",ActionRow,"")&&m.event()==Accept){upgrades(v,false);sn::SET_VEHICLE_TYRES_CAN_BURST(v,true);}
  m.description("Removes all mods and restores normal tyres.");
  return true;
 }
 // Menyoo SetVehicleMaxUpgrades(vehicle, upgradeIt, invincible=false, plate unchanged).
 void upgrades(int v,bool upgrade){
  if(sn::GET_VEHICLE_MOD_KIT(v)!=0)sn::SET_VEHICLE_MOD_KIT(v,0);tw::setInvincible(v,false);
  sn::TOGGLE_VEHICLE_MOD(v,18,upgrade);sn::TOGGLE_VEHICLE_MOD(v,20,upgrade);sn::TOGGLE_VEHICLE_MOD(v,22,upgrade);
  for(int i=0;i<49;++i){if(i>=17&&i<=22)continue;
   if(i==24){const int front=sn::GET_VEHICLE_MOD(v,23);sn::SET_VEHICLE_MOD(v,i,upgrade?front:-1,false);continue;}
   int index=sn::GET_NUM_VEHICLE_MODS(v,i)-1;if(index>-1)index=rand()%(index+2)-1;if(i>=11&&i<=16)index=sn::GET_NUM_VEHICLE_MODS(v,i)-1;
   sn::SET_VEHICLE_MOD(v,i,upgrade?index:-1,false);}
  sn::SET_VEHICLE_WINDOW_TINT(v,upgrade?1:0);sn::SET_VEHICLE_TYRES_CAN_BURST(v,false);
 }
 bool bennysPage(Transport&m){
  if(!valid())return true;const int v=target_;m.setTitle(tw::text("S_MO_09","Benny's Lowrider Mods"));
  if(m.add("Interior Colour",ActionRow,"",ids_.shared)&&m.event()==Accept)paintPart_=5;
  if(m.add("Dashboard Colour",ActionRow,"",ids_.shared)&&m.event()==Accept)paintPart_=6;
  for(int i=25;i<=48;++i)if(sn::GET_NUM_VEHICLE_MODS(v,i)>0&&m.add(slotName(v,i),ActionRow,"",ids_.catall)&&m.event()==Accept)modSlot_=i;
  return true;
 }
 bool catallPage(Transport&m){
  if(!valid())return true;const int v=target_;const int maxMod=sn::GET_NUM_VEHICLE_MODS(v,modSlot_)-1,current=sn::GET_VEHICLE_MOD(v,modSlot_);
  m.setTitle(slotName(v,modSlot_));const bool bike=bikeModel(tw::model(v));
  for(int i=-1;i<=maxMod;++i)if(tick(m,modLabel(v,modSlot_,i),current==i,bike?BikeIcon:CarIcon)){
   if(i<0)sn::REMOVE_VEHICLE_MOD(v,modSlot_);else sn::SET_VEHICLE_MOD(v,modSlot_,i,sn::GET_VEHICLE_MOD_VARIATION(v,modSlot_));}
  return true;
 }
 bool paintsPage(Transport&m){
  if(!valid())return true;const int v=target_;char t[32];m.setTitle("Paints");
  if(m.add(tw::text("CMOD_COL0_0","Primary"),ActionRow,"",ids_.paints2)&&m.event()==Accept)paintPart_=1;
  if(m.add(tw::text("CMOD_COL0_1","Secondary"),ActionRow,"",ids_.paints2)&&m.event()==Accept)paintPart_=2;
  if(m.add(tw::text("CMOD_COL1_6","Pearlescent"),ActionRow,"",ids_.pearl)&&m.event()==Accept)paintPart_=3;
  if(m.add(tw::text("CMOD_MOD_WHEM","Wheels"),ActionRow,"",ids_.shared)&&m.event()==Accept){paintPart_=4;
   if(sn::GET_VEHICLE_MOD(v,23)<0)tw::notify("Colours cannot always be applied to stock wheels.");}
  if(m.add("Interior Colour",ActionRow,"",ids_.shared)&&m.event()==Accept)paintPart_=5;
  if(m.add("Dashboard Colour",ActionRow,"",ids_.shared)&&m.event()==Accept)paintPart_=6;
  m.add("---Collateral---",BreakRow,"");
  float fade=sn::GET_VEHICLE_ENVEFF_SCALE(v);sprintf_s(t,"%.2f",fade);int d=stepper(m,"Paint Fade",t,"How worn and faded the paint looks.");
  if(d){fade+=.02f*d;if(fade>1.f)fade=1.f;if(fade<0.f)fade=0.f;sn::SET_VEHICLE_ENVEFF_SCALE(v,fade);}
  float dirt=sn::GET_VEHICLE_DIRT_LEVEL(v);sprintf_s(t,"%.2f",dirt);d=stepper(m,"Dirt Level",t);
  if(d){dirt+=.1f*d;if(dirt>15.f)dirt=15.f;if(dirt<0.f)dirt=0.f;sn::SET_VEHICLE_DIRT_LEVEL(v,dirt);}
  int combo=sn::GET_VEHICLE_COLOUR_COMBINATION(v)+1;sprintf_s(t,"%d",combo);d=stepper(m,"CarVariation Colours",t,"Colour combinations built into this model. Clears custom colours.");
  if(d){const int n=sn::GET_NUMBER_OF_VEHICLE_COLOURS(v);combo+=d;if(combo>n)combo=1;if(combo<1)combo=n;
   sn::CLEAR_VEHICLE_CUSTOM_PRIMARY_COLOUR(v);sn::CLEAR_VEHICLE_CUSTOM_SECONDARY_COLOUR(v);sn::SET_VEHICLE_COLOUR_COMBINATION(v,combo-1);}
  return true;
 }
 bool paints2Page(Transport&m){
  const bool spawn=spawnPaints();if(!spawn&&!valid())return true;const int v=target_;
  m.setTitle(paintPart_==1||paintPart_==10?tw::text("CMOD_COL0_0","Primary"):tw::text("CMOD_COL0_1","Secondary"));
  if(spawn&&tick(m,"None (Default)",part(paintPart_)==-3,TickIcon))paint(paintPart_,-3,-1);
  m.add("Chrome",ActionRow,"",ids_.chrome);m.add("Classic",ActionRow,"",ids_.classic);m.add("Matte",ActionRow,"",ids_.matte);
  m.add("Metallic",ActionRow,"",ids_.metallic);m.description("Standard metallic car paints.");
  m.add("Metal",ActionRow,"",ids_.metal);m.description("Brushed and bare metal finishes, like steel, aluminium and gold.");
  if(sn::IS_DLC_PRESENT(sn::GET_HASH_KEY("spchameleon")))m.add("Chameleon",ActionRow,"",ids_.chameleon);
  m.add("Utility",ActionRow,"",ids_.util);m.description("Unpainted utility-vehicle finishes.");
  m.add("Worn",ActionRow,"",ids_.worn);m.description("Weathered, aged paint finishes.");
  indexSlider(m);
  if(m.add("Random Index",ActionRow,"")&&m.event()==Accept){loadPaints();if(spawn){int total=0;for(int i=0;i<5;++i)total+=sn::GET_NUM_MOD_COLORS(i,true);const int top=160+(total-232);paint(paintPart_,top>0?rand()%top:0,-1);return true;}int maximum=0;for(int k=0;k<7;++k)for(size_t i=0;i<paints_[k].size();++i){if(paints_[k][i].paint>maximum)maximum=paints_[k][i].paint;}
   paint(paintPart_,maximum>0?rand()%maximum:0,-1);}
  m.description("Applies a random paint from this category.");
  if(spawn)return true; // RGB/copy rows edit a vehicle's custom paint, not the spawn index setting
  if(m.add("Random RGB",ActionRow,"")&&m.event()==Accept){paint(paintPart_,0,-1);
   if(paintPart_==1)sn::SET_VEHICLE_CUSTOM_PRIMARY_COLOUR(v,rand()%255,rand()%255,rand()%255);else sn::SET_VEHICLE_CUSTOM_SECONDARY_COLOUR(v,rand()%255,rand()%255,rand()%255);}
  if(m.add("Set RGB",ActionRow,"",ids_.rgb)&&m.event()==Accept)rgbMode_=paintPart_==1?0:1;
  if(m.add(paintPart_==1?"Copy to Secondary":"Copy to Primary",ActionRow,"")&&m.event()==Accept){
   paint(3-paintPart_,part(paintPart_),-1);int r=0,g=0,b=0;
   if(paintPart_==1&&sn::GET_IS_VEHICLE_PRIMARY_COLOUR_CUSTOM(v)){sn::GET_VEHICLE_CUSTOM_PRIMARY_COLOUR(v,&r,&g,&b);sn::SET_VEHICLE_CUSTOM_SECONDARY_COLOUR(v,r,g,b);}
   if(paintPart_==2&&sn::GET_IS_VEHICLE_SECONDARY_COLOUR_CUSTOM(v)){sn::GET_VEHICLE_CUSTOM_SECONDARY_COLOUR(v,&r,&g,&b);sn::SET_VEHICLE_CUSTOM_PRIMARY_COLOUR(v,r,g,b);}}
  m.description("Copies this paint to the other (primary/secondary) layer.");
  return true;
 }
 // Paint type order of GET_NUM_MOD_COLORS: 0 metallic,1 classic,2 pearl,3 matte,4 metal,5 chrome,6 chameleon.
 bool listPage(Transport&m,int which){
  if(!spawnPaints()&&!valid())return true;loadPaints();
  switch(which){case 0:paintList(m,"Chrome",paints_[5],false);break;case 1:paintList(m,"Classic",paints_[1],false);break;
   case 2:paintList(m,"Matte",paints_[3],false);break;case 3:paintList(m,"Metallic",paints_[0],false);break;
   case 4:paintList(m,"Metal",paints_[4],false);break;case 5:paintList(m,"Chameleon",paints_[6],false);break;
   case 6:paintPart_=3;paintList(m,"Pearlescent",paints_[2],true);indexSlider(m);break;
   case 7:fixedList(m,"Utility",customs::PAINTS_UTIL,26);break;case 8:fixedList(m,"Worn",customs::PAINTS_WORN,26);break;}
  return true;
 }
 bool sharedPage(Transport&m){
  if(!valid())return true;
  switch(paintPart_){case 5:fixedList(m,"Interior",customs::PAINTS_INTERIOR,68);break;case 6:fixedList(m,"Dashboard",customs::PAINTS_DASHBOARD,49);break;
   case 3:loadPaints();paintList(m,"Pearlescent",paints_[2],true);break;default:paintPart_=4;fixedList(m,tw::text("CMOD_MOD_WHEM","Wheels").c_str(),customs::PAINTS_WHEELS,41);break;}
  indexSlider(m);return true;
 }
 bool rgbPage(Transport&m){
  const bool spawn=spawnRgb();if(!spawn&&!valid())return true;const int v=target_;int r=0,g=0,b=0;
  if(spawn){r=spawnNeon_[0];g=spawnNeon_[1];b=spawnNeon_[2];}
  else switch(rgbMode_){case 0:sn::GET_VEHICLE_CUSTOM_PRIMARY_COLOUR(v,&r,&g,&b);break;case 1:sn::GET_VEHICLE_CUSTOM_SECONDARY_COLOUR(v,&r,&g,&b);break;
   case 2:sn::GET_VEHICLE_NEON_COLOUR(v,&r,&g,&b);break;case 4:sn::GET_VEHICLE_TYRE_SMOKE_COLOR(v,&r,&g,&b);break;}
  m.setTitle("Set Colour");char t[16];
  if(rgbMode_==0||rgbMode_==1){static const customs::Paint finish[10]={{"Standard Metallic",2,-1},{"Dark Metallic",0,-1},{"Bright Metallic",111,-1},{"Matte",12,-1},
    {"Util",15,-1},{"Worn",21,-1},{"Brushed Metal",117,-1},{"Pure Chrome",120,-1},{"Coloured Chrome",158,-1},{"Satin",159,-1}};
   const int current=part(rgbMode_+1);int f=0;for(int i=0;i<10;++i)if(finish[i].paint==current)f=i;
   const int d=choice(m,"Paint Finish",finish[f].name);
   if(d){f=(f+d+10)%10;paint(rgbMode_+1,finish[f].paint,-1);if(rgbMode_==0)sn::SET_VEHICLE_CUSTOM_PRIMARY_COLOUR(v,r,g,b);else sn::SET_VEHICLE_CUSTOM_SECONDARY_COLOUR(v,r,g,b);}}
  int*c[3]={&r,&g,&b};const char*n[3]={"Red","Green","Blue"};
  for(int i=0;i<3;++i){sprintf_s(t,"%d",*c[i]);const int d=stepper(m,n[i],t);if(d){*c[i]=(*c[i]+d+256)%256;setRgb(r,g,b);}}
  static int hud=0;const bool hudChosen=m.add("HUD Colour",ChoiceRow,customs::HUD_COLOURS[hud]);m.description("Picks a colour from the game's HUD colour presets.");
  if(hudChosen&&(m.event()==Left||m.event()==Right))hud=(hud+(m.event()==Right?1:-1)+216)%216;
  else if(hudChosen&&m.event()==Accept){int a=0;sn::GET_HUD_COLOUR(hud,&r,&g,&b,&a);setRgb(r,g,b);}
  m.add("---Presets---",BreakRow,"");
  for(int i=0;i<13;++i){const customs::Preset&p=customs::NEON_PRESETS[i];if(tick(m,p.name,r==p.r&&g==p.g&&b==p.b,TickIcon))setRgb(p.r,p.g,p.b);}
  return true;
 }
 bool wheelsPage(Transport&m){
  if(!valid())return true;const int v=target_;const bool bike=bikeModel(tw::model(v));
  m.setTitle(tw::text("CMOD_MOD_WHEM","Wheels"));
  if(m.add(tw::text("CMOD_MOD_WCL","Rim Colour"),ActionRow,"",ids_.shared)&&m.event()==Accept){paintPart_=4;if(sn::GET_VEHICLE_MOD(v,23)<0)tw::notify("Colours cannot always be applied to stock wheels.");}
  if(tick(m,tw::text("CMOD_TYR_0","Stock Wheels"),sn::GET_VEHICLE_MOD(v,23)<0)){sn::REMOVE_VEHICLE_MOD(v,23);sn::REMOVE_VEHICLE_MOD(v,24);}
  for(int i=0;i<13;++i){const bool bikeWheels=i==6;if(bike!=bikeWheels)continue;
   if(m.add(wheelTypeName(i),ActionRow,"",bikeWheels?ids_.wheels2:ids_.wheels3)&&m.event()==Accept){wheelType_=i;wheelRear_=0;if(!bikeWheels)sn::SET_VEHICLE_WHEEL_TYPE(v,i);}}
  const int wheel=sn::GET_VEHICLE_MOD(v,23);const bool custom=sn::GET_VEHICLE_MOD_VARIATION(v,23)!=0;
  if(toggle(m,tw::text("CMOD_TYR_1","Custom Tyres"),custom,"Uses the custom tyre style for the current wheels.")){sn::SET_VEHICLE_MOD(v,23,wheel,!custom);sn::SET_VEHICLE_MOD(v,24,wheel,!custom);}
  const bool bulletproof=!sn::GET_VEHICLE_TYRES_CAN_BURST(v);
  if(toggle(m,tw::text("CMOD_TYR_2","Bulletproof Tyres"),bulletproof))sn::SET_VEHICLE_TYRES_CAN_BURST(v,bulletproof);
  const bool drift=sn::GET_DRIFT_TYRES_SET(v);if(toggle(m,"Drift Tyres",drift,"Low-grip tyres for drifting."))sn::SET_DRIFT_TYRES(v,!drift);
  if(m.add(tw::text("CMOD_MOD_TYR3","Tire Smoke Colour"),ActionRow,"",ids_.rgb)&&m.event()==Accept)rgbMode_=4;
  return true;
 }
 bool wheels2Page(Transport&m){if(!valid())return true;m.setTitle(wheelTypeName(wheelType_));
  if(m.add("Front",ActionRow,"",ids_.wheels3)&&m.event()==Accept){wheelRear_=0;sn::SET_VEHICLE_WHEEL_TYPE(target_,wheelType_);}
  if(m.add("Rear",ActionRow,"",ids_.wheels3)&&m.event()==Accept){wheelRear_=1;sn::SET_VEHICLE_WHEEL_TYPE(target_,wheelType_);}return true;}
 bool wheels3Page(Transport&m){
  if(!valid())return true;const int v=target_;const int slot=wheelRear_?24:23;
  const int count=sn::GET_NUM_VEHICLE_MODS(v,slot);m.setTitle(wheelType_==6?"Bike Wheels":"Normal Wheels");
  const int type=sn::GET_VEHICLE_WHEEL_TYPE(v),current=sn::GET_VEHICLE_MOD(v,slot);const bool bike=bikeModel(tw::model(v));
  for(int i=0;i<count;++i)if(tick(m,modLabel(v,23,i),type==wheelType_&&current==i,bike?BikeIcon:CarIcon)){
   sn::SET_VEHICLE_WHEEL_TYPE(v,wheelType_);
   if(wheelType_==6)sn::SET_VEHICLE_MOD(v,slot,i,sn::GET_VEHICLE_MOD_VARIATION(v,slot));
   else {sn::SET_VEHICLE_MOD(v,23,i,sn::GET_VEHICLE_MOD_VARIATION(v,23));sn::SET_VEHICLE_MOD(v,24,i,sn::GET_VEHICLE_MOD_VARIATION(v,24));}}
  return true;
 }
 bool neonsPage(Transport&m){
  if(!valid())return true;const int v=target_;m.setTitle(tw::text("PIM_PVEO_004","Neons Lights"));
  const char*sides[4]={"Left","Right","Front","Back"};
  for(int i=0;i<4;++i){neonState_[i]=sn::GET_VEHICLE_NEON_ENABLED(v,i);if(tick(m,sides[i],neonState_[i])){neonState_[i]=!neonState_[i];sn::SET_VEHICLE_NEON_ENABLED(v,i,neonState_[i]);}}
  if(m.add("Set Colour",ActionRow,"",ids_.rgb)&&m.event()==Accept)rgbMode_=2;
  if(toggle(m,"Neon RGB",neonRgb_,"Cycles the neon colour."))neonRgb_=!neonRgb_;
  static const char*fade[5]={"None","Simple","Heartbeat","Shift","Slide"};static const char*flash[5]={"None","Simple","Spin","SpinBack","Firework"};
  int d=choice(m,"Neon Fade",fade[neonFade_],"Fading animation pattern for neons.");if(d)neonFade_=(neonFade_+d+5)%5;
  d=choice(m,"Neon Flash",flash[neonFlash_],"Flashing animation pattern for neons.");if(d)neonFlash_=(neonFlash_+d+5)%5;
  if(neonFade_||neonFlash_){char t[16];sprintf_s(t,"%d",neonDelay_);d=stepper(m,"Animation Speed (ms)",t,"Time per step of the neon animation. Lower is faster.");
   if(d){neonDelay_+=50*d;if(neonDelay_<50)neonDelay_=50;if(neonDelay_>10000)neonDelay_=10000;}}
  return true;
 }
 bool doorsPage(Transport&m){
  if(!valid())return true;const int v=target_;m.setTitle("Doors");
  static const char*locks[11]={"None","Unlocked","Locked","LockedForPlayer","ChildLock","Unknown 5","Unknown 6","CanBeBrokenInto","CanBeBrokenIntoPersist","Unknown 9","CannotBeTriedToBeEntered"};
  int lock=sn::GET_VEHICLE_DOOR_LOCK_STATUS(v);if(lock<0||lock>10)lock=0;int d=choice(m,"Lock Status",locks[lock]);
  if(d){lock+=d;if(lock>=0&&lock<=10)sn::SET_VEHICLE_DOORS_LOCKED(v,lock);}
  static const char*actions[4]={"Open/Close","Close","Remove","Fix"};d=choice(m,"Action",actions[doorAction_],"What pressing a door does: open/close, remove or fix.");
  if(d)doorAction_=(doorAction_+d+4)%4;
  static const char*names[6]={"Driver Door","Passenger Door","Rear Left","Rear Right","Hood","Trunk"};
  for(int i=0;i<6;++i){const bool open=doorOpen(i);if(tick(m,names[i],doorAction_==0?open:false,TickIcon))doorAct(i,doorAction_==0&&open?1:doorAction_);}
  bool all=true;for(int i=0;i<6;++i)all=all&&doorOpen(i);
  if(tick(m,"All",doorAction_==0&&all,TickIcon))for(int i=0;i<7;++i)doorAct(i,doorAction_==0&&all?1:doorAction_);
  return true;
 }
 void doorAct(int door,int action){switch(action){case 0:sn::SET_VEHICLE_DOOR_OPEN(target_,door,false,false);break;case 1:sn::SET_VEHICLE_DOOR_SHUT(target_,door,false);break;
   case 2:sn::SET_DOOR_ALLOWED_TO_BE_BROKEN_OFF(target_,door,true);sn::SET_VEHICLE_DOOR_BROKEN(target_,door,true);break;case 3:fixDoor(door);break;}}
 // Menyoo GTAvehicle::FixDoor (retail CVehicle::GetDoorByID + door fix pattern) == source SET_VEHICLE_DOOR_CONTROL(DT_DOOR_INTACT),
 // which calls CCarDoor::Fix -> RestoreAbove for a detached door (commands_vehicle.cpp CommandSetVehicleDoorControl).
 void fixDoor(int door){if(door>=0&&door<6)sn::SET_VEHICLE_DOOR_CONTROL(target_,door,0,0.f);}
 bool windowsPage(Transport&m){
  if(!valid())return true;const int v=target_;m.setTitle("Windows");
  static const char*tints[7]={"None","Black","CMOD_WIN_2","CMOD_WIN_1","Stock","CMOD_WIN_3","Green"};
  int tint=sn::GET_VEHICLE_WINDOW_TINT(v);if(tint<0||tint>6)tint=0;int d=choice(m,tw::text("CMOD_GLD2_2","Window Tint"),tw::text(tints[tint],tints[tint]));
  if(d){tint+=d;if(tint<0)tint=0;if(tint>6)tint=6;sn::SET_VEHICLE_WINDOW_TINT(v,tint);}
  m.add("---Status---",BreakRow,"");
  static const char*modes[5]={"Open","Close","Break","Fix","Remove"};d=choice(m,"Action",modes[windowMode_],"What pressing a window does: open, close, break, fix or remove.");
  if(d){windowMode_+=d;if(windowMode_<0)windowMode_=0;if(windowMode_>4)windowMode_=4;}
  static const char*names[4]={"Front Left","Front Right","Back Left","Back Right"};
  for(int i=0;i<4;++i)if(m.add(names[i],ActionRow,"")&&m.event()==Accept)window(i);
  if(m.add("All Windows",ActionRow,"")&&m.event()==Accept)for(int i=0;i<4;++i)window(i);
  return true;
 }
 void window(int i){switch(windowMode_){case 0:sn::ROLL_DOWN_WINDOW(target_,i);break;case 1:sn::ROLL_UP_WINDOW(target_,i);break;case 2:sn::SMASH_VEHICLE_WINDOW(target_,i);break;
   case 3:sn::FIX_VEHICLE_WINDOW(target_,i);break;case 4:sn::REMOVE_VEHICLE_WINDOW(target_,i);break;}}
 bool emblemPage(Transport&m){
  if(!valid())return true;const int v=target_;m.setTitle(tw::text("CMOD_COL0_3","Emblem"));
  if(tick(m,tw::text("FMMC_REM","CLEAR"),true,CrossIcon))sn::REMOVE_DECALS_FROM_VEHICLE(v);
  if(m.add("Player Crew Emblem",ActionRow,"")&&m.event()==Accept){const int me=tw::ped();const int bone=sn::GET_ENTITY_BONE_INDEX_BY_NAME(v,"chassis_dummy");
   tw::V3 front,back;tw::extents(tw::model(v),front,back);
   if(!sn::ADD_VEHICLE_CREW_EMBLEM(v,me,bone,tw::v3(0,front.y*.4f,front.z*.9f),tw::v3(0,0,-1.f),tw::v3(0,1.f,0),.6f,0,200))tw::notify("No crew emblem is available offline for this player.");}
  m.description("Adds the player's crew emblem (needs crew data; Story offline usually has none).");
  return true;
 }
 bool removablePage(Transport&m){
  if(!valid())return true;const int v=target_;m.setTitle("Removable Components");refreshCache();
  for(size_t i=0;i<brokenDoors_.size();++i)if(!sn::IS_VEHICLE_DOOR_DAMAGED(v,brokenDoors_[i])){sn::SET_DOOR_ALLOWED_TO_BE_BROKEN_OFF(v,brokenDoors_[i],true);sn::SET_VEHICLE_DOOR_BROKEN(v,brokenDoors_[i],true);}
  const std::vector<int>ds=doors();static const char*doorNames[7]={"Front Left Door","Front Right Door","Back Left Door","Back Right Door","Hood","Trunk","Trunk 2"};
  if(!ds.empty())m.add("Doors",BreakRow,"");
  for(size_t i=0;i<ds.size();++i){const bool broken=has(brokenDoors_,ds[i]);
   if(choice(m,doorNames[ds[i]],broken?"Detached":"Attached")){if(broken){fixDoor(ds[i]);erase(brokenDoors_,ds[i]);}else {sn::SET_DOOR_ALLOWED_TO_BE_BROKEN_OFF(v,ds[i],true);sn::SET_VEHICLE_DOOR_BROKEN(v,ds[i],true);brokenDoors_.push_back(ds[i]);}}}
  bool extras=false;for(int id=0;id<=20;++id)if(sn::DOES_EXTRA_EXIST(v,id)){if(!extras){m.add("Extras",BreakRow,"");extras=true;}char n[16];sprintf_s(n,"Extra %d",id);
   const bool on=sn::IS_VEHICLE_EXTRA_TURNED_ON(v,id);if(tick(m,n,on,BoxTickIcon))sn::SET_VEHICLE_EXTRA(v,id,on);}
  if(tw::heli(tw::model(v))){m.add("Helicopter Parts",BreakRow,"");if(tick(m,"Break Rudder",false,CrossIcon))sn::SET_VEHICLE_RUDDER_BROKEN(v,true);}
  m.add("Tyres",BreakRow,"");static const char*tyres[6]={"Front Left","Front Right","Middle Left","Middle Right","Back Left","Back Right"};
  static const char*tyreBones[6]={"wheel_lf","wheel_rf","wheel_lm1","wheel_rm1","wheel_lr","wheel_rr"};
  for(int i=0;i<6;++i){if(!tw::bone(v,tyreBones[i]))continue;const bool burst=has(burstTyres_,i);
   if(choice(m,tyres[i],burst?"Deflated":"Intact")){if(burst){sn::SET_VEHICLE_TYRE_FIXED(v,i);erase(burstTyres_,i);}else {sn::SET_VEHICLE_TYRES_CAN_BURST(v,true);sn::SET_VEHICLE_TYRE_BURST(v,i,true,1000.f);burstTyres_.push_back(i);}}}
  return true;
 }
 bool lightsPage(Transport&m){
  if(!valid())return true;const int v=target_;m.setTitle(tw::text("CMOD_MOD_LGT","Lights"));
  int lightsOn=0,highBeams=0;sn::GET_VEHICLE_LIGHTS_STATE(v,&lightsOn,&highBeams);
  if(tick(m,"Headlights",lightsOn==1,BoxTickIcon))sn::SET_VEHICLE_LIGHTS(v,lightsOn==1?4:3);
  if(tick(m,"Left Indicator",leftInd_,BoxTickIcon)){rightInd_=hazard_=false;sn::SET_VEHICLE_INDICATOR_LIGHTS(v,1,!leftInd_);sn::SET_VEHICLE_INDICATOR_LIGHTS(v,0,false);leftInd_=!leftInd_;}
  if(tick(m,"Right Indicator",rightInd_,BoxTickIcon)){leftInd_=hazard_=false;sn::SET_VEHICLE_INDICATOR_LIGHTS(v,0,!rightInd_);sn::SET_VEHICLE_INDICATOR_LIGHTS(v,1,false);rightInd_=!rightInd_;}
  if(tick(m,"Hazard Lights",hazard_,BoxTickIcon)){const bool next=(rightInd_==leftInd_)?!hazard_:true;rightInd_=leftInd_=false;
   sn::SET_VEHICLE_INDICATOR_LIGHTS(v,0,next);sn::SET_VEHICLE_INDICATOR_LIGHTS(v,1,next);hazard_=next;}
  return true;
 }
 // Engine sound: Select Vehicle through the installed vehicle catalogue (Input Model is REMOVE).
 template<typename Catalogue> bool engineSoundPage(Transport&m,Catalogue&vehicles){
  m.setTitle(tw::text("CMM_MOD_G3","Engine")+" "+tw::text("VEUI_AUD_TIT","Sound"));if(!valid())return true;
  if(!vehicles.load()){m.add("Unavailable",ActionRow,"");m.rows.back().enabled=false;return true;}
  for(unsigned g=0;g<23;++g){const unsigned n=vehicles.count(g);if(!n)continue;char c[16];sprintf_s(c,"%u",n);
   if(m.add(Catalogue::categoryName(g),ActionRow,c,ids_.engineSoundItems)&&m.event()==Accept)soundGroup_=static_cast<int>(g);}
  return true;
 }
 template<typename Catalogue> bool engineSoundItemsPage(Transport&m,Catalogue&vehicles){
  m.setTitle(Catalogue::categoryName(static_cast<unsigned>(soundGroup_)));if(!valid())return true;
  for(unsigned i=0;i<vehicles.count(static_cast<unsigned>(soundGroup_));++i){const auto*e=vehicles.entry(static_cast<unsigned>(soundGroup_),i);
   if(e&&m.add(e->name,ActionRow,"")&&m.event()==Accept)sn::FORCE_USE_AUDIO_GAME_OBJECT(target_,e->name);}
  return true;
 }
 // ---------------------------------------------------------------- per-UPDATE animation tick (neons) and multipliers
 void tick(){
  const int now=tw::timer();
  for(std::map<int,float>::iterator i=torque_.begin();i!=torque_.end();){if(!tw::exists(i->first)){i=torque_.erase(i);continue;}sn::SET_VEHICLE_CHEAT_POWER_INCREASE(i->first,i->second);++i;}
  for(std::map<int,float>::iterator i=maxSpeed_.begin();i!=maxSpeed_.end();){if(!tw::exists(i->first)){i=maxSpeed_.erase(i);continue;}sn::SET_ENTITY_MAX_SPEED(i->first,i->second);++i;}
  if(!valid()||(!neonRgb_&&!neonFade_&&!neonFlash_))return;const int v=target_;
  int r=neonR_,g=neonG_,b=neonB_;if(neonRgb_){const tw::Fader&f=tw::fader();r=f.r;g=f.g;b=f.b;}
  const int delay=neonDelay_<50?50:neonDelay_;const float pi=3.142f;const int t=now%(2*delay);float k=1.f;
  switch(neonFade_){
   case 1:k=t>0?.5f*(cosf(pi*t/delay)+1.f):1.f;break;
   case 2:{const int h=now%delay;k=h<delay/2?1.f-fabsf(cosf(pi*h/delay*4.f)):0.f;break;}
   case 3:r=static_cast<int>((96-.75f*r)*sinf(pi*t/delay)+96+.25f*r);g=static_cast<int>((96-.75f*g)*sinf(pi*t/delay)+96+.25f*g);b=static_cast<int>((96-.75f*b)*sinf(pi*t/delay)+96+.25f*b);break;
   case 4:{const float s=.5f*pi*t/delay;k=(fabsf(1.2f*tanhf(1.2f*sinf(s)))-1.f)*floorf(cosf(pi*(static_cast<float>(t/delay)+.5f)))+1.2f*(-fabsf(tanhf(1.2f*cosf(s)))*floorf(sinf(pi*t/delay)));break;}
  }
  if(neonFade_==1||neonFade_==2||neonFade_==4){r=static_cast<int>(r*k);g=static_cast<int>(g*k);b=static_cast<int>(b*k);}
  neonColour(r<0?0:(r>255?255:r),g<0?0:(g>255?255:g),b<0?0:(b>255?255:b));
  if(neonFlash_==1&&now-flashTick_>=delay){flashOn_=!flashOn_;flashTick_=now;}
  if((neonFlash_==2||neonFlash_==3)&&now-spinTick_>delay/4){static const int next[4]={2,3,1,0},back[4]={3,2,0,1};spin_=next[spin_];spinBack_=back[spinBack_];spinTick_=now;}
  if(neonFlash_==4){const int step=(now%delay)/(delay/20>0?delay/20:1);const bool all=step==9||step==12||step==15;
   fwk_[0]=fwk_[1]=all||step==3||step==4;fwk_[2]=all||step==1||step==2;fwk_[3]=all||step==5||step==6;}
  for(int i=0;i<4;++i){bool on=neonState_[i];
   if(neonFlash_==1)on=on&&flashOn_;else if(neonFlash_==2)on=i==spin_;else if(neonFlash_==3)on=i==spinBack_;else if(neonFlash_==4)on=fwk_[i];
   sn::SET_VEHICLE_NEON_ENABLED(v,i,on);}
 }
};
}
