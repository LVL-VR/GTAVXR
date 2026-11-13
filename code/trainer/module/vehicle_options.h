#pragma once
// retained Menyoo Vehicle Options (curtis VehicleOptions.cpp + Routine.cpp TickVehicleEffects, VehicleCruise,
// VehicleFly, AutoDrive, Slam It, Vehicle Weapons, Multipliers, Multi-Platform Neons). REMOVE rows (Speedometers,
// PV Options, Opacity, Tow Mode, Vehicle Jump, Hydraulics, Collision) have no row, dispatch, tick or input here.
// Quest adaptations (no desktop keys/keybinds): Vehicle Boost = hold horn (original Race Boost On Horn);
// vehicle weapons fire = left stick click (original controller binding INPUT_FRONTEND_LS); Fly Mode and the
// handling multiplier use the original controller branch; numeric text entry is replaced by steppers.
#include "trainer_world.h"
#include "reset.h"
#include "menu_transport.h"
#include <stdio.h>
namespace menyoo_vr {
struct VehiclePageIds {int spawner,customs,weapons,multipliers,neons,neonColour,autodrive,slam;};
class VehicleOptions {
 VehiclePageIds ids_;bool permitted_;
 // Toggles bound to the reset ledger (Reset Toggles stops them; the tick then runs each mode's own off path).
 enum Toggle { DisableSiren,Invincible,AutoRepair,AutoFlip,EngineOn,Rainbow,Cruise,Boost,NativeBoost,SuprKar,Fly,Glue,
  DriveWater,HeavyMass,Neons,NeonRainbow,WeaponLines,AutoDrive,AutoRandom,AutoPush,Numeric,ToggleCount };
 enum Weapon { WRpg,WFireworks,WGuns,WSnowballs,WBalls,WHydrant,WFlame,WGreenLaser,WRedLaser,WValkyrie,WFlares,WSniper,WTazer,WMolotov,WPdw,WeaponCount };
 bool on_[ToggleCount],was_[ToggleCount],weapons_[WeaponCount];
 int fixMode_,accel_,brake_,handling_,styleIndex_;float damage_,slam_,neonIntensity_,autoSpeed_,pushRadius_,cruiseSpeed_;
 int neonR_,neonG_,neonB_;bool slidy_,noGravity_,freeze_;
 int lastVehicle_,waterObject_,rappelStep_,rappelPed_,rappelVehicle_,rappelAt_,fireStep_,fireVehicle_,fireAt_,cargoStep_,cargoAt_;
 int autoVehicle_;bool autoSet_;tw::V3 autoTarget_;std::vector<int>neonVehicles_;
 static const char*weaponName(unsigned i){static const char*n[WeaponCount]={"RPG","Fireworks","Guns","Snowballs","Balls","Water Hydrants","Flame Leaks",
  "Green Laser","Red Laser","Valkyrie Turrets","Flares","Heavy Snipers","Tazers","Molotovs","Combat PDWs"};return i<WeaponCount?n[i]:"";}
 static unsigned weaponHash(unsigned i){static const unsigned h[WeaponCount]={3204302209u,2138347493u,3220176749u,126349499u,600439132u,0u,0u,
  4026335563u,1566990507u,1097917585u,1233104067u,205991906u,911657153u,615608432u,171789620u};return i<WeaponCount?h[i]:0u;}
 static float weaponSpeed(unsigned i){return i==WSnowballs||i==WBalls||i==WFlares||i==WTazer||i==WMolotov?2970.f:2000.f;}
 struct Style {const char*name;int value;};
 static const Style&style(unsigned i){static const Style s[6]={{"Normal",786603},{"Ignore Lights",2883621},{"Sometimes Overtake Traffic",5},
  {"Rushed",1074528293},{"Avoid Traffic",786468},{"Avoid Traffic A Lot",6}};return s[i<6?i:0];}
 struct Slam {const char*name;float value;};
 static const Slam&slamValue(unsigned i){static const Slam s[11]={{"Off",0.f},{"I'm A Skyscraper",.35f},{"Such High",.30f},{"I'm Tall, Aren't I",.25f},
  {"I'm Growing",.20f},{"Tip Toeing",.10f},{"I Got The Crabs",-.10f},{"Get Get Get, Get Low",-.20f},{"Short Is Cute",-.25f},
  {"I'm A Ninja Turtle",-.30f},{"Apple Bottom Jeans",-.38f}};return s[i<11?i:0];}
 static void unavailable(Transport&m,const char*why){m.rows.back().enabled=false;m.description(why);}
 bool toggleRow(Transport&m,const char*label,bool&value,const char*help){
  const bool chosen=m.add(label,ToggleRow,value?"ON":"OFF");if(help)m.description(help);
  if(chosen&&m.event()==Accept){value=!value;return true;}return false;
 }
 bool localRow(Transport&m,const char*label,bool state,const char*help){
  const bool chosen=m.add(label,ToggleRow,state?"ON":"OFF");if(help)m.description(help);return chosen&&m.event()==Accept;
 }
 bool actionRow(Transport&m,const char*label,const char*help,int submenu=-1){
  const bool chosen=m.add(label,ActionRow,"",submenu);if(help)m.description(help);return chosen&&m.event()==Accept;
 }
 int stepper(Transport&m,const char*label,const char*value,const char*help){
  const bool chosen=m.add(label,NumberRow,value);if(help)m.description(help);
  if(!chosen)return 0;return m.event()==Right?1:(m.event()==Left?-1:0);
 }
 void applyDamage(){const int p=tw::player();sn::SET_PLAYER_VEHICLE_DAMAGE_MODIFIER(p,damage_);sn::SET_PLAYER_VEHICLE_DEFENSE_MODIFIER(p,damage_);}
 static bool hasSiren(int v){return tw::bone(v,"siren1");}
 // Fix & Wash with the caption semantics: 0 Full, 1 Keep Dirt, 2 Keep windows open, 3 Keep windows open with dirt.
 void fixAndWash(int v){
  const bool keepDirt=fixMode_==1||fixMode_==3,keepWindows=fixMode_==2||fixMode_==3;
  const float dirt=sn::GET_VEHICLE_DIRT_LEVEL(v);bool open[8]={};
  if(keepWindows)for(int i=0;i<8;++i)open[i]=!sn::IS_VEHICLE_WINDOW_INTACT(v,i);
  sn::SET_VEHICLE_FIXED(v);sn::SET_VEHICLE_DIRT_LEVEL(v,keepDirt?dirt:0.f);sn::SET_VEHICLE_ENGINE_CAN_DEGRADE(v,false);
  sn::SET_VEHICLE_ENGINE_HEALTH(v,1250.f);sn::SET_VEHICLE_PETROL_TANK_HEALTH(v,1250.f);sn::SET_VEHICLE_BODY_HEALTH(v,1250.f);
  sn::SET_VEHICLE_UNDRIVEABLE(v,false);if(!sn::GET_IS_VEHICLE_ENGINE_RUNNING(v))sn::SET_VEHICLE_ENGINE_ON(v,true,true,false);
  if(keepWindows)for(int i=0;i<8;++i)if(open[i])sn::ROLL_DOWN_WINDOW(v,i);
 }
 void teleportClosest(){const int p=tw::ped();const int v=tw::closest(QueryVehicles,tw::position(p),2000.f,tw::vehicle());
  if(!v){tw::notify("No vehicle nearby.");return;}sn::SET_PED_INTO_VEHICLE(p,v,tw::firstFreeSeat(v,-1));}
 void deleteVehicle(int v){int handle=v;sn::SET_ENTITY_AS_MISSION_ENTITY(handle,true,true);sn::DELETE_VEHICLE(&handle);tw::notify("Poof!");}
public:
 VehicleOptions():permitted_(false){memset(&ids_,0,sizeof(ids_));resetState();}
 void resetState(){
  memset(on_,0,sizeof(on_));memset(was_,0,sizeof(was_));memset(weapons_,0,sizeof(weapons_));
  on_[AutoPush]=true;fixMode_=accel_=brake_=handling_=styleIndex_=0;damage_=1.f;slam_=0.f;neonIntensity_=3.5f;autoSpeed_=20.f;pushRadius_=4.f;cruiseSpeed_=0.f;
  neonR_=255;neonG_=0;neonB_=255;slidy_=noGravity_=freeze_=false;lastVehicle_=waterObject_=0;rappelStep_=rappelPed_=rappelVehicle_=rappelAt_=0;
  fireStep_=fireVehicle_=fireAt_=cargoStep_=cargoAt_=0;autoVehicle_=0;autoSet_=false;autoTarget_=tw::v3(0,0,0);neonVehicles_.clear();
 }
 bool bind(const VehiclePageIds&ids,ResetLedger&ledger){
  ids_=ids;permitted_=false;resetState();
  for(unsigned i=0;i<ToggleCount;++i)if(i!=AutoPush&&!ledger.bindToggle(on_[i],NULL))return false;
  for(unsigned i=0;i<WeaponCount;++i)if(!ledger.bindToggle(weapons_[i],NULL))return false;
  return true;
 }
 void permit(bool enabled){permitted_=enabled;}
 // ---------------------------------------------------------------- pages
 bool page(Transport&m){
  m.setTitle("Vehicle Options");const int me=tw::ped(),v=tw::vehicle();const unsigned model=v?tw::model(v):0;
  if(v){static const char*modes[4]={"Full","Keep Dirt","Keep windows open","Keep windows open with Dirt"};
   const bool chosen=m.add("Fix & Wash",ChoiceRow,modes[fixMode_]);
   m.description("Repairs the vehicle. Left/right chooses whether to keep dirt or open windows.");
   if(chosen){if(m.event()==Right&&fixMode_<3)++fixMode_;else if(m.event()==Left&&fixMode_>0)--fixMode_;else if(m.event()==Accept)fixAndWash(v);}
  }
  m.add("Vehicle Spawner",ActionRow,"",ids_.spawner);
  if(actionRow(m,"Menyoo Customs",NULL,ids_.customs)&&!v)tw::notify("You are not in a vehicle.");
  if(v&&tw::cargobob(model)){const bool magnet=sn::DOES_CARGOBOB_HAVE_PICKUP_MAGNET(v);
   if(localRow(m,"Cargobob Magnet",magnet,NULL)){
    if(magnet){sn::REMOVE_PICK_UP_ROPE_FOR_CARGOBOB(v);cargoStep_=1;cargoAt_=tw::timer()+50;}
    else {sn::CREATE_PICK_UP_ROPE_FOR_CARGOBOB(v,1);if(sn::DOES_CARGOBOB_HAVE_PICKUP_MAGNET(v))sn::SET_CARGOBOB_PICKUP_MAGNET_ACTIVE(v,true);}
   }}
  if(v&&sn::DOES_VEHICLE_ALLOW_RAPPEL(v)&&actionRow(m,"Rappel From Helicopter",NULL)&&!rappelStep_){rappelVehicle_=v;rappelStep_=1;}
  if(v&&hasSiren(v))toggleRow(m,"Disable Vehicle Siren",on_[DisableSiren],NULL);
  if(v&&tw::boat(model)){const bool anchored=sn::IS_BOAT_ANCHORED(v);
   if(localRow(m,"Anchor Boat",anchored,NULL)&&sn::CAN_ANCHOR_BOAT_HERE(v)){sn::SET_BOAT_ANCHOR(v,!anchored);sn::SET_BOAT_REMAINS_ANCHORED_WHILE_PLAYER_IS_DRIVER(v,!anchored);}}
  if(actionRow(m,"Teleport Into Closest Vehicle",NULL))teleportClosest();
  m.add("Vehicle Weapons",ActionRow,"",ids_.weapons);
  m.add("Vehicle Multipliers",ActionRow,"",ids_.multipliers);
  m.add("Multi-Platform Neons",ActionRow,"",ids_.neons);m.description("Draws neon light under any vehicle, including bikes and boats.");
  m.add("Auto Drive",ActionRow,"",ids_.autodrive);m.description("Your vehicle drives itself to the waypoint or to random destinations.");
  m.add("Slam It",ActionRow,"",ids_.slam);m.description("Pushes the vehicle down for a lower stance.");
  char text[48];sprintf_s(text,"%.2f",damage_);
  const int d=stepper(m,"Damage & Defense",text,"Multiplies damage your vehicle deals and resists. 1.0 is normal.");
  if(d){damage_+=.2f*d;if(damage_<0.f)damage_=0.f;on_[Numeric]=true;applyDamage();}
  toggleRow(m,"Invincibility (Looped)",on_[Invincible],"Keeps the vehicle invincible and repaired while enabled.");
  const bool invisible=v&&!sn::IS_ENTITY_VISIBLE(v);
  if(localRow(m,"Invisibility",invisible,NULL)){if(!v)tw::notify("You are not in a vehicle.");
   else {const bool pedVisible=sn::IS_ENTITY_VISIBLE(me);sn::SET_ENTITY_VISIBLE(v,invisible,false);sn::SET_ENTITY_VISIBLE(me,pedVisible,false);}}
  toggleRow(m,"Auto-Repair",on_[AutoRepair],NULL);
  toggleRow(m,"Auto-Flip",on_[AutoFlip],"Puts the vehicle back on its wheels when it lands upside down.");
  toggleRow(m,"Keep Engine & Lights On",on_[EngineOn],"Stops the engine and lights turning off.");
  const bool killed=v&&sn::GET_VEHICLE_ENGINE_HEALTH(v)<0.f;
  if(localRow(m,"Kill Engine",killed,"Destroys the engine so the vehicle won't drive.")){if(!v)tw::notify("You are not in a vehicle.");
   else if(!killed){sn::SET_VEHICLE_ENGINE_HEALTH(v,-1.f);sn::SET_VEHICLE_UNDRIVEABLE(v,true);tw::notify("Engine Killed");}
   else {sn::SET_VEHICLE_ENGINE_HEALTH(v,1250.f);sn::SET_VEHICLE_UNDRIVEABLE(v,false);tw::notify("Engine Revived");}}
  toggleRow(m,"Rainbow Mode",on_[Rainbow],"Cycles the paint through colours.");
  if(localRow(m,"Slidy Tyres",slidy_,"Greatly reduces tyre grip.")){if(!v)tw::notify("You are not in a vehicle.");else {slidy_=!slidy_;sn::SET_VEHICLE_REDUCE_GRIP(v,slidy_);}}
  if(toggleRow(m,"Cruise Control",on_[Cruise],"Holds the current speed. Accelerate/brake adjust it by 2 km/h.")&&on_[Cruise])cruiseSpeed_=0.f;
  toggleRow(m,"Vehicle Boost",on_[Boost],"Hold the horn (left stick click) for a speed boost.");
  toggleRow(m,"Infinite Native Boost (for e.g. Oppressor)",on_[NativeBoost],"Keeps rocket boost full on vehicles that have one (e.g. Oppressor, Voltic). Hold horn.");
  if(toggleRow(m,"SuprKar Mode",on_[SuprKar],"Much faster acceleration and instant braking while on the ground.")&&!on_[SuprKar]&&v)sn::SET_VEHICLE_BOOST_ACTIVE(v,false);
  toggleRow(m,"Fly Mode",on_[Fly],"Accelerate up, Brake down, left stick to move. The vehicle follows your view.");
  toggleRow(m,"Glue to Ground",on_[Glue],"Keeps the vehicle snapped to the ground, preventing flips.");
  toggleRow(m,"Drive On Water",on_[DriveWater],"Lets the vehicle drive on the water surface.");
  toggleRow(m,"Increased Mass",on_[HeavyMass],"Pushes other vehicles away at speed; your vehicle stays invincible and repaired.");
  const int lock=v?sn::GET_VEHICLE_DOOR_LOCK_STATUS(v):0;
  if(localRow(m,"Child Locks",lock==4,"Passengers can't get out.")&&v)sn::SET_VEHICLE_DOORS_LOCKED(v,lock==4?0:4);
  if(localRow(m,"Door Locks",lock==2,NULL)&&v)sn::SET_VEHICLE_DOORS_LOCKED(v,lock==2?0:2);
  if(localRow(m,"No Gravity",noGravity_,NULL)){if(!v)tw::notify("You are not in a vehicle.");
   else {noGravity_=!noGravity_;sn::SET_VEHICLE_GRAVITY(v,!noGravity_);tw::notify(noGravity_?"Vehicle Gravity Disabled":"Vehicle Gravity Enabled");}}
  if(localRow(m,"Freeze Vehicle",freeze_,NULL)){if(!v)tw::notify("You are not in a vehicle.");
   else {freeze_=!freeze_;sn::SET_VEHICLE_FORWARD_SPEED(v,freeze_?0.f:8.f);sn::FREEZE_ENTITY_POSITION(v,freeze_);tw::notify(freeze_?"Vehicle Freeze On":"Vehicle Freeze Off");}}
  const bool burning=v&&sn::IS_ENTITY_ON_FIRE(v);
  if(localRow(m,"Set on Fire",burning,NULL)){if(!v)tw::notify("You are not in a vehicle.");
   else if(!fireStep_){sn::SET_ENTITY_PROOFS(v,true,!burning?false:true,true,false,false,false,false,true);fireVehicle_=v;fireStep_=burning?2:1;fireAt_=tw::timer()+40;}}
  if(actionRow(m,"Delete Vehicle",NULL)){if(!v)tw::notify("You are not in a vehicle.");else deleteVehicle(v);}
  return true;
 }
 bool weaponsPage(Transport&m){
  m.setTitle("Vehicle Weapons");toggleRow(m,"Display Projectile Path",on_[WeaponLines],NULL);
  if(actionRow(m,"Turn All Off",NULL))memset(weapons_,0,sizeof(weapons_));
  for(unsigned i=0;i<WeaponCount;++i){
   if(toggleRow(m,weaponName(i),weapons_[i],i==0?"Hold left stick click to fire forward (look behind fires backwards).":NULL)&&weapons_[i]){
    const unsigned h=weaponHash(i);if(h&&!sn::HAS_WEAPON_ASSET_LOADED(static_cast<int>(h)))sn::REQUEST_WEAPON_ASSET(static_cast<int>(h),31,0);}
  }
  return true;
 }
 bool multipliersPage(Transport&m){
  m.setTitle("Multipliers");char t[32];
  sprintf_s(t,"%d",accel_);int d=stepper(m,"Acceleration",t,"Multiplier on top of the vehicle's normal acceleration.");
  if(d){accel_+=d;if(accel_<0)accel_=0;if(accel_>200)accel_=200;on_[Numeric]=true;}
  sprintf_s(t,"%d",brake_);d=stepper(m,"Braking & Reverse",t,"Multiplier on top of the vehicle's normal braking and reverse.");
  if(d){brake_+=d;if(brake_<0)brake_=0;if(brake_>100)brake_=100;on_[Numeric]=true;}
  sprintf_s(t,"%d",handling_);d=stepper(m,"Handling",t,"Multiplier on top of the vehicle's normal handling and traction.");
  if(d){handling_+=d;if(handling_<0)handling_=0;if(handling_>100)handling_=100;on_[Numeric]=true;}
  return true;
 }
 bool neonsPage(Transport&m){
  m.setTitle("Neons");if(toggleRow(m,"Toggle",on_[Neons],"Draws neon light under any vehicle, including bikes and boats.")&&!on_[Neons])neonVehicles_.clear();
  char t[32];sprintf_s(t,"%.2f",neonIntensity_);const int d=stepper(m,"Intensity",t,"Brightness of the neon light.");
  if(d){neonIntensity_+=.05f*d;if(neonIntensity_<0.f)neonIntensity_=0.f;}
  toggleRow(m,"Rainbow Mode",on_[NeonRainbow],NULL);
  m.add("Set Colour",ActionRow,"",ids_.neonColour);return true;
 }
 bool neonColourPage(Transport&m){
  m.setTitle("Set Colour");int*c[3]={&neonR_,&neonG_,&neonB_};const char*n[3]={"Red","Green","Blue"};char t[16];
  for(unsigned i=0;i<3;++i){sprintf_s(t,"%d",*c[i]);const int d=stepper(m,n[i],t,NULL);if(d){*c[i]+=d*5;if(*c[i]<0)*c[i]=0;if(*c[i]>255)*c[i]=255;}}
  return true;
 }
 bool autoDrivePage(Transport&m){
  m.setTitle("Auto Drive");
  if(toggleRow(m,"Random Destination",on_[AutoRandom],"Auto Drive picks random destinations instead of the waypoint.")){on_[AutoDrive]=on_[AutoRandom];autoSet_=false;}
  if(localRow(m,"Go To Waypoint",on_[AutoDrive]&&!on_[AutoRandom],NULL)){on_[AutoRandom]=false;on_[AutoDrive]=!on_[AutoDrive];autoSet_=false;}
  char t[32];sprintf_s(t,"%.1f",autoSpeed_*3.6f);int d=stepper(m,"Speed (KMPH)",t,NULL);
  if(d){autoSpeed_+=d*.5f/3.6f;if(autoSpeed_<0.f)autoSpeed_=0.f;sn::SET_DRIVE_TASK_CRUISE_SPEED(tw::ped(),autoSpeed_);}
  const bool chosen=m.add("Driving Style",ChoiceRow,style(styleIndex_).name);
  if(chosen&&(m.event()==Left||m.event()==Right)){if(m.event()==Right&&styleIndex_<5)++styleIndex_;else if(m.event()==Left&&styleIndex_>0)--styleIndex_;
   sn::SET_DRIVE_TASK_DRIVING_STYLE(tw::ped(),style(styleIndex_).value);}
  toggleRow(m,"Push Other Vehicles Away",on_[AutoPush],"Pushes nearby vehicles out of the way while driving.");
  sprintf_s(t,"%.1f",pushRadius_);d=stepper(m,"Forcefield Radius",t,"Range of the push.");
  if(d){pushRadius_+=.5f*d;if(pushRadius_<1.f)pushRadius_=1.f;}
  return true;
 }
 bool slamPage(Transport&m){
  m.setTitle("Slam It");
  for(unsigned i=0;i<11;++i){const Slam&s=slamValue(i);if(m.add(s.name,ActionRow,"")&&m.event()==Accept){slam_=s.value;on_[Numeric]=true;
    if(s.value<=-.35f)tw::notify("You can even drive on walls with this value.");}
   m.icons(slam_==s.value,CarIcon,NoIcon);}
  return true;
 }
 // ---------------------------------------------------------------- tick (once per UPDATE, visible or not)
 bool tick(){
  if(!permitted_)return false;const int now=tw::timer();tw::fader().tick(now);
  const int me=tw::ped(),v=tw::vehicle();
  // Off transitions run each mode's own restoration exactly once (also after Reset Toggles).
  if(was_[Invincible]&&!on_[Invincible]&&tw::exists(lastVehicle_))tw::setInvincible(lastVehicle_,false);
  if(was_[DisableSiren]&&!on_[DisableSiren]&&tw::exists(lastVehicle_))sn::SET_VEHICLE_HAS_MUTED_SIRENS(lastVehicle_,false);
  if(was_[HeavyMass]&&!on_[HeavyMass]&&tw::exists(lastVehicle_)){tw::setInvincible(lastVehicle_,false);sn::SET_VEHICLE_FRICTION_OVERRIDE(lastVehicle_,1.f);}
  if(was_[DriveWater]&&!on_[DriveWater]&&waterObject_){int o=waterObject_;if(tw::exists(o)){sn::SET_ENTITY_AS_MISSION_ENTITY(o,true,true);sn::DELETE_OBJECT(&o);}waterObject_=0;}
  if(was_[SuprKar]&&!on_[SuprKar]&&tw::exists(lastVehicle_))sn::SET_VEHICLE_BOOST_ACTIVE(lastVehicle_,false);
  if(was_[NativeBoost]&&!on_[NativeBoost]&&tw::exists(lastVehicle_)&&sn::GET_HAS_ROCKET_BOOST(lastVehicle_))sn::SET_ROCKET_BOOST_ACTIVE(lastVehicle_,false);
  if(was_[AutoDrive]&&!on_[AutoDrive]){sn::CLEAR_PED_TASKS(me);autoSet_=false;}
  if(was_[Numeric]&&!on_[Numeric]){accel_=brake_=handling_=0;slam_=0.f;damage_=1.f;applyDamage();}
  if(was_[Neons]&&!on_[Neons])neonVehicles_.clear();
  memcpy(was_,on_,sizeof(on_));
  // Timed one-shot steps (Menyoo WAIT replaced by game-timer steps; no blocking inside the page).
  if(cargoStep_&&now>=cargoAt_){if(v&&tw::cargobob(tw::model(v)))sn::CREATE_PICK_UP_ROPE_FOR_CARGOBOB(v,0);cargoStep_=0;}
  if(fireStep_&&now>=fireAt_){if(tw::exists(fireVehicle_)){if(fireStep_==1)sn::START_ENTITY_FIRE(fireVehicle_);else sn::STOP_ENTITY_FIRE(fireVehicle_);}fireStep_=0;}
  rappelTick(now);
  if(on_[Neons])neonsTick(v);
  if(!v){noGravity_=freeze_=slidy_=false;if(on_[DriveWater])driveOnWaterTick(me);if(on_[Cruise])on_[Cruise]=false;return true;}
  lastVehicle_=v;const unsigned model=tw::model(v);
  const bool driver=sn::GET_PED_IN_VEHICLE_SEAT(v,-1,false)==me;
  if(!tw::heli(model)&&(tw::plane(model)||sn::IS_VEHICLE_ON_ALL_WHEELS(v))&&driver){
   if(accel_&&sn::IS_CONTROL_PRESSED(2,tw::INPUT_VEH_ACCELERATE))tw::force(v,1,tw::v3(0,accel_/69.f,0),tw::v3(0,0,0),0,true,true,true,false,true);
   if(brake_&&sn::IS_CONTROL_PRESSED(2,tw::INPUT_VEH_BRAKE))tw::force(v,0,tw::v3(0,static_cast<float>(-brake_),0),tw::v3(0,0,0),0,true,true,true,false,true);
   if(handling_&&sn::GET_ENTITY_SPEED_VECTOR(v,true).y>2.f){const float x=sn::GET_DISABLED_CONTROL_NORMAL(2,tw::INPUT_SCRIPT_LEFT_AXIS_X);
    if(x>.5f||x<-.5f)tw::force(v,1,tw::v3(handling_*x/220.f,0,0),tw::v3(0,0,0),0,true,true,true,false,true);}
  }
  if(damage_!=1.f)applyDamage();
  if(on_[EngineOn]){if(!sn::GET_IS_VEHICLE_ENGINE_RUNNING(v))sn::SET_VEHICLE_ENGINE_ON(v,true,true,false);sn::SET_VEHICLE_LIGHTS(v,3);}
  if(on_[Invincible])tw::setInvincible(v,true);
  if(on_[AutoRepair]&&sn::GET_DOES_VEHICLE_HAVE_DAMAGE_DECALS(v))tw::repair(v,2000.f);
  if(on_[AutoFlip]){const float roll=sn::GET_ENTITY_ROLL(v);
   if(sn::IS_ENTITY_UPSIDEDOWN(v)&&(roll>160.f||roll<-160.f)&&!sn::IS_ENTITY_IN_AIR(v)&&!sn::IS_ENTITY_IN_WATER(v)&&!tw::plane(model)&&!tw::heli(model)){
    const tw::V3 r=sn::GET_ENTITY_ROTATION(v,2);sn::SET_ENTITY_ROTATION(v,tw::v3(0,0,r.z),2,true);}}
  if(on_[Rainbow]){const tw::Fader&f=tw::fader();sn::SET_VEHICLE_CUSTOM_PRIMARY_COLOUR(v,f.r,f.g,f.b);sn::SET_VEHICLE_CUSTOM_SECONDARY_COLOUR(v,f.r,f.g,f.b);}
  if(on_[DisableSiren]&&hasSiren(v))sn::SET_VEHICLE_HAS_MUTED_SIRENS(v,true);
  if(slam_!=0.f&&(slam_<=-.35f||sn::IS_VEHICLE_ON_ALL_WHEELS(v)))tw::force(v,1,tw::v3(0,0,slam_),tw::v3(0,0,0),1,true,true,true,false,true);
  if(on_[HeavyMass])heavyMassTick(v);
  if(on_[DriveWater])driveOnWaterTick(v);
  if(sn::IS_PAUSE_MENU_ACTIVE())return true;
  if(on_[Boost]&&sn::IS_CONTROL_PRESSED(2,tw::INPUT_VEH_HORN)){sn::SET_VEHICLE_BOOST_ACTIVE(v,true);
   tw::force(v,1,tw::v3(0,1.7f,0),tw::v3(0,0,0),0,true,true,true,false,true);sn::SET_VEHICLE_BOOST_ACTIVE(v,false);}
  if(on_[NativeBoost]&&sn::GET_HAS_ROCKET_BOOST(v)){if(sn::IS_CONTROL_PRESSED(2,tw::INPUT_VEH_HORN)){sn::SET_ROCKET_BOOST_FILL(v,1.f);sn::SET_ROCKET_BOOST_ACTIVE(v,true);}
   else sn::SET_ROCKET_BOOST_ACTIVE(v,false);}
  if(on_[Glue])sn::SET_VEHICLE_ON_GROUND_PROPERLY(v,0.f);
  if(on_[SuprKar]&&sn::IS_VEHICLE_ON_ALL_WHEELS(v)){
   if(sn::IS_CONTROL_PRESSED(2,tw::INPUT_VEH_ACCELERATE)){sn::SET_VEHICLE_BOOST_ACTIVE(v,true);sn::SET_VEHICLE_BOOST_ACTIVE(v,false);
    sn::SET_VEHICLE_FORWARD_SPEED(v,sn::GET_ENTITY_SPEED_VECTOR(v,true).y+.46f);}
   if(sn::IS_CONTROL_JUST_PRESSED(2,tw::INPUT_VEH_BRAKE))sn::SET_VEHICLE_FORWARD_SPEED(v,0.f);
   else if(sn::IS_CONTROL_PRESSED(2,tw::INPUT_VEH_BRAKE))tw::force(v,1,tw::v3(0,-.4f,0),tw::v3(0,0,0),0,true,true,true,false,true);
  }
  if(on_[Cruise])cruiseTick(v,model,me);
  if(on_[Fly])flyTick(v,model);
  if(on_[AutoDrive])autoDriveTick(v,model,me);
  vehicleWeaponsTick(v,model);
  return true;
 }
 void suspend(){memset(on_,0,sizeof(on_));memset(weapons_,0,sizeof(weapons_));}
 // Close/Reset All: release this mode's own auxiliary entities (never gameplay entities).
 bool release(){int o=waterObject_;if(o&&tw::exists(o)){sn::SET_ENTITY_AS_MISSION_ENTITY(o,true,true);sn::DELETE_OBJECT(&o);}waterObject_=0;
  if(rappelPed_&&tw::exists(rappelPed_)){int p=rappelPed_;sn::SET_PED_AS_NO_LONGER_NEEDED(&p);}rappelPed_=0;rappelStep_=0;return true;}
private:
 void cruiseTick(int v,unsigned model,int me){
  if(tw::heli(model)){on_[Cruise]=false;return;}const bool isPlane=tw::plane(model);
  if(cruiseSpeed_==0.f)cruiseSpeed_=sn::GET_ENTITY_SPEED_VECTOR(v,true).y;
  if(cruiseSpeed_<.01f){on_[Cruise]=false;cruiseSpeed_=0.f;return;}
  const bool inSeat=isPlane||sn::GET_PED_IN_VEHICLE_SEAT(v,-1,false)==me;
  if(!inSeat||(!isPlane&&(!sn::IS_VEHICLE_ON_ALL_WHEELS(v)||sn::IS_ENTITY_IN_WATER(v))))return;
  sn::SET_VEHICLE_FORWARD_SPEED(v,cruiseSpeed_);
  if(sn::IS_CONTROL_JUST_PRESSED(2,tw::INPUT_VEH_BRAKE))cruiseSpeed_-=2.f/3.6f;
  if(sn::IS_CONTROL_JUST_PRESSED(2,tw::INPUT_VEH_ACCELERATE))cruiseSpeed_+=2.f/3.6f;
 }
 void flyTick(int v,unsigned model){
  const int list[4]={tw::INPUT_VEH_ACCELERATE,tw::INPUT_VEH_BRAKE,tw::INPUT_VEH_HANDBRAKE,tw::INPUT_SPRINT};
  for(int i=0;i<4;++i)sn::DISABLE_CONTROL_ACTION(2,list[i],true);
  sn::SET_ENTITY_ROTATION(v,sn::GET_GAMEPLAY_CAM_ROT(2),2,true);
  const float up=sn::GET_DISABLED_CONTROL_NORMAL(2,tw::INPUT_VEH_ACCELERATE),down=sn::GET_DISABLED_CONTROL_NORMAL(2,tw::INPUT_VEH_BRAKE);
  const float y=-sn::GET_DISABLED_CONTROL_NORMAL(2,tw::INPUT_SCRIPT_LEFT_AXIS_Y),x=sn::GET_DISABLED_CONTROL_NORMAL(2,tw::INPUT_SCRIPT_LEFT_AXIS_X);
  (void)model;
  if(up)tw::pushRelative(v,tw::v3(0,0,up));if(down)tw::pushRelative(v,tw::v3(0,0,-down));
  if(y>0.f)tw::pushRelative(v,tw::v3(0,y,0));else if(y<0.f)tw::pushRelative(v,tw::v3(0,y,0));
  if(x!=0.f)tw::pushRelative(v,tw::v3(1.2f*x,0,0));
 }
 void heavyMassTick(int v){
  const float speed=sn::GET_ENTITY_SPEED(v);if(speed<.5f)return;
  tw::setInvincible(v,true);if(sn::GET_DOES_VEHICLE_HAVE_DAMAGE_DECALS(v))sn::SET_VEHICLE_FIXED(v);sn::SET_VEHICLE_FRICTION_OVERRIDE(v,100.f);
  const int driver=sn::GET_PED_IN_VEHICLE_SEAT(v,-1,false);if(driver>0)sn::SET_PED_CONFIG_FLAG(driver,32,false);
  std::vector<int>around;tw::nearby(QueryVehicles,tw::position(v),30.f,around);const tw::V3 normal=sn::GET_COLLISION_NORMAL_OF_LAST_HIT_FOR_ENTITY(v);
  for(size_t i=0;i<around.size();++i)if(around[i]!=v&&sn::IS_ENTITY_TOUCHING_ENTITY(v,around[i])){tw::push(around[i],tw::mul(normal,speed*3.5f));sn::SET_VEHICLE_FORWARD_SPEED(v,speed);}
 }
 void driveOnWaterTick(int subject){
  if(!waterObject_||!tw::exists(waterObject_)){
   const unsigned objModel=0xC42C019Au; // prop_ld_ferris_wheel
   if(!tw::requestModel(objModel))return;
   const tw::V3 p=tw::offset(subject,0,4.f,0);float height=0.f;
   if(sn::GET_WATER_HEIGHT_NO_WAVES(p,&height))sn::SET_ENTITY_COORDS(subject,tw::v3(p.x,p.y,height),false,false,false,true);
   waterObject_=sn::CREATE_OBJECT(static_cast<int>(objModel),tw::v3(p.x,p.y,height-4.f),true,true,true);
   if(!waterObject_)return;sn::SET_ENTITY_COORDS_NO_OFFSET(waterObject_,tw::v3(p.x,p.y,height),false,false,false);
   sn::SET_ENTITY_ROTATION(waterObject_,tw::v3(0,90.f,0),2,true);sn::FREEZE_ENTITY_POSITION(waterObject_,true);
   sn::SET_MODEL_AS_NO_LONGER_NEEDED(static_cast<int>(objModel));tw::notify("Enable again if water level is incorrect/changes.");return;
  }
  const tw::V3 mine=tw::position(subject),pos=tw::position(waterObject_);
  if(sn::IS_ENTITY_IN_WATER(subject)){float height=0.f;if(sn::GET_WATER_HEIGHT_NO_WAVES(pos,&height))sn::SET_ENTITY_COORDS_NO_OFFSET(waterObject_,tw::v3(pos.x,pos.y,height),false,false,false);}
  sn::SET_ENTITY_COORDS_NO_OFFSET(waterObject_,tw::v3(mine.x,mine.y,pos.z),true,true,true);
  sn::SET_ENTITY_ROTATION(waterObject_,tw::v3(180.f,90.f,180.f),2,true);sn::SET_ENTITY_VISIBLE(waterObject_,false,false);sn::FREEZE_ENTITY_POSITION(waterObject_,true);
 }
 void neonsTick(int v){
  if(on_[NeonRainbow]){const tw::Fader&f=tw::fader();neonR_=f.r;neonG_=f.g;neonB_=f.b;}
  if(v){bool known=false;for(size_t i=0;i<neonVehicles_.size();++i)if(neonVehicles_[i]==v){known=true;break;}if(!known)neonVehicles_.push_back(v);}
  for(size_t i=0;i<neonVehicles_.size();){const int n=neonVehicles_[i];
   if(!tw::exists(n)||sn::IS_ENTITY_DEAD(n,false)){neonVehicles_.erase(neonVehicles_.begin()+static_cast<long>(i));continue;}
   tw::V3 front,back;tw::extents(tw::model(n),front,back);
   const tw::V3 points[4]={tw::offset(n,0,front.y-.3f,-.4f),tw::offset(n,0,.3f-back.y,-.4f),tw::offset(n,front.x-.3f,.64f,-.4f),tw::offset(n,.3f-back.x,.64f,-.4f)};
   for(int k=0;k<4;++k)sn::DRAW_LIGHT_WITH_RANGE(points[k],neonR_,neonG_,neonB_,5.f,neonIntensity_);
   ++i;}
 }
 void rappelTick(int now){
  if(!rappelStep_)return;const int me=tw::ped();const int v=rappelVehicle_;
  if(!tw::exists(v)){rappelStep_=0;return;}
  if(rappelStep_==1){sn::CLEAR_PED_TASKS_IMMEDIATELY(me);sn::SET_PED_INTO_VEHICLE(me,v,1);rappelPed_=0;
   if(sn::IS_VEHICLE_SEAT_FREE(v,-1,false)){const unsigned driverModel=0x855E36A3u;if(!tw::requestModel(driverModel))return;
    rappelPed_=sn::CREATE_PED_INSIDE_VEHICLE(v,26,static_cast<int>(driverModel),-1,true,true);sn::SET_MODEL_AS_NO_LONGER_NEEDED(static_cast<int>(driverModel));}
   rappelStep_=2;rappelAt_=now+100;return;}
  if(rappelStep_==2&&now>=rappelAt_){
   if(rappelPed_&&tw::exists(rappelPed_)){const tw::V3 p=tw::position(v);sn::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(rappelPed_,true);
    sn::TASK_HELI_MISSION(rappelPed_,v,0,0,p,4,0.f,50.f,-1.f,10000,100,-1.f,0);sn::SET_PED_KEEP_TASK(rappelPed_,true);}
   sn::SET_ENTITY_VELOCITY(v,tw::v3(0,0,0));sn::TASK_RAPPEL_FROM_HELI(me,10.f);rappelStep_=3;rappelAt_=now+100;return;}
  if(rappelStep_==3&&now>=rappelAt_){if(rappelPed_&&tw::exists(rappelPed_)){int p=rappelPed_;sn::SET_PED_AS_NO_LONGER_NEEDED(&p);}rappelPed_=0;rappelStep_=0;}
 }
 void autoDriveTick(int v,unsigned model,int me){
  if(!(sn::IS_WAYPOINT_ACTIVE()||on_[AutoRandom])){on_[AutoDrive]=false;return;}
  if(v!=autoVehicle_){autoVehicle_=v;autoSet_=false;}
  const tw::V3 pos=tw::position(v);
  const bool outOfBounds=pos.x>4000.f||pos.x<-4000.f||pos.y>7000.f||pos.y<-3500.f;
  if(!autoSet_&&sn::IS_WAYPOINT_ACTIVE())autoTarget_=sn::GET_BLIP_INFO_ID_COORD(sn::GET_FIRST_BLIP_INFO_ID(8));
  if(on_[AutoRandom]&&(tw::heli(model)||tw::plane(model))&&(tw::distance(pos,autoTarget_)<300.f||outOfBounds||!autoSet_)){
   if(outOfBounds)autoTarget_=tw::v3(0,0,400.f);
   else {const int dir=sn::GET_RANDOM_INT_IN_RANGE(1,5);const float o=4000.f;
    autoTarget_=dir==1?tw::v3(pos.x,pos.y+o,400.f):dir==2?tw::v3(pos.x+o,pos.y,400.f):dir==3?tw::v3(pos.x,pos.y-o,400.f):tw::v3(pos.x-o,pos.y,400.f);}
   autoSet_=false;}
  if(!autoSet_){int task=0;sn::OPEN_SEQUENCE_TASK(&task);const int st=style(styleIndex_).value;
   if(tw::heli(model))sn::TASK_HELI_MISSION(0,v,0,0,autoTarget_,9,autoSpeed_,250.f,-1.f,100,200,-1.f,0);
   else if(tw::plane(model))sn::TASK_PLANE_MISSION(0,v,0,0,autoTarget_,4,autoSpeed_,50.f,-1.f,100,200,false);
   else if(tw::boat(model)&&!on_[AutoRandom])sn::TASK_BOAT_MISSION(0,v,0,0,autoTarget_,4,autoSpeed_,786469,10.f,1071);
   else if(on_[AutoRandom])sn::TASK_VEHICLE_DRIVE_WANDER(0,v,autoSpeed_,st);
   else sn::TASK_VEHICLE_DRIVE_TO_COORD_LONGRANGE(0,v,autoTarget_,autoSpeed_,st,7.f);
   sn::CLOSE_SEQUENCE_TASK(task);sn::TASK_PERFORM_SEQUENCE(me,task);sn::CLEAR_SEQUENCE_TASK(&task);autoSet_=true;}
  if(on_[AutoPush]){tw::V3 front,back;tw::extents(model,front,back);const tw::V3 dir=tw::direction(sn::GET_ENTITY_ROTATION(v,2));
   const tw::V3 bumper=tw::add(pos,tw::mul(dir,front.y));std::vector<int>around;tw::nearby(QueryVehicles|QueryPeds|QueryObjects,bumper,pushRadius_+3.2f,around);
   for(size_t i=0;i<around.size();++i)if(around[i]!=v&&around[i]!=me&&tw::distance(tw::position(around[i]),bumper)<=pushRadius_)tw::push(around[i],tw::mul(dir,10.f));}
 }
 void vehicleWeaponsTick(int v,unsigned model){
  bool any=false;for(unsigned i=0;i<WeaponCount;++i)any=any||weapons_[i];if(!any&&!on_[WeaponLines])return;
  tw::V3 front,back;tw::extents(model,front,back);const bool behind=sn::IS_DISABLED_CONTROL_PRESSED(0,tw::INPUT_LOOK_BEHIND);
  const tw::V3 originR=behind?tw::offset(v,front.x-.22f,.5f-back.y,.5f):tw::offset(v,front.x-.22f,front.y-.5f,.5f);
  const tw::V3 targetR=behind?tw::offset(v,front.x-.22f,-(front.y+350.f),.5f):tw::offset(v,front.x-.22f,front.y+350.f,.5f);
  const tw::V3 originL=behind?tw::offset(v,.22f-back.x,.5f-back.y,.5f):tw::offset(v,.22f-back.x,front.y-.5f,.5f);
  const tw::V3 targetL=behind?tw::offset(v,.22f-back.x,-(back.y+350.f),.5f):tw::offset(v,.22f-back.x,front.y+350.f,.5f);
  if(on_[WeaponLines]){sn::DRAW_LINE(originR,targetR,255,40,40,255);sn::DRAW_LINE(originL,targetL,255,40,40,255);}
  if(!any||!sn::IS_CONTROL_PRESSED(2,tw::INPUT_FRONTEND_LS))return;
  sn::CLEAR_AREA_OF_PROJECTILES(originR,8.f,0);const int me=tw::ped();
  for(unsigned i=0;i<WeaponCount;++i){if(!weapons_[i])continue;
   if(i==WHydrant||i==WFlame){const int type=i==WHydrant?13:30;
    sn::ADD_EXPLOSION(tw::offset(v,1.24f,behind?-40.f:40.f,0.f),type,5.f,true,true,.3f,false);
    sn::ADD_EXPLOSION(tw::offset(v,-1.24f,behind?-40.f:40.f,0.f),type,5.f,true,true,.3f,false);continue;}
   const int h=static_cast<int>(weaponHash(i));
   sn::SHOOT_SINGLE_BULLET_BETWEEN_COORDS(originR,targetR,200,true,h,me,true,false,weaponSpeed(i));
   sn::SHOOT_SINGLE_BULLET_BETWEEN_COORDS(originL,targetL,200,true,h,me,true,false,weaponSpeed(i));
  }
 }
};
}
