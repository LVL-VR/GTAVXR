#pragma once
// retained Menyoo Weapon Options (curtis WeaponOptions.cpp Weaponops/Individual Weapons/Attachments & Tints/
// Parachute/Favourites/Loadouts/Laser Sight; Routine.cpp weapon ticks: Infinite Parachutes, Weapon Damage, Tenfold
// Bullets, Bullet Time, Triggerbot, Rapid Fire, Soul-Switch, Teleport/Revival/Removal guns; GTAped Store/GiveWeapons;
// WeaponIndivs.cpp catalogue via weapon_catalogue_data.h). REMOVE rows (Digiscanner, Briefcase, Rope, Magnet,
// Flamethrower, Forge, Gravity, Light, TriggerFX, Kaboom, Bullet, Ped and Object/Vehicle guns, Add New Weapon,
// Create New Folder, SEARCH, Rename File) have no row, page, tick or input here.
// VR adaptations: Menyoo aims these modes with the flat gameplay-camera ray; in VR the weapon is aimed by the hand, so
// the same modes use the held weapon's muzzle and barrel axis (CWeapon::GetMuzzleMatrix uses the weapon X axis) and
// are not gated on flat free-aim. Favourites and loadout files take the installed weapon caption / a generated name.
#include "trainer_world.h"
#include "teleport_actions.h"
#include "weapon_catalogue_data.h"
#include "vehicle_customs_data.h"
#include "menu_transport.h"
#include "reset.h"
#include "pugixml.hpp"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm>
namespace menyoo_vr {
struct WeaponPageIds {int indivs,category,item,mods,parachute,smoke,favourites,loadouts,loadout,laser,laserColour;};
// Rows whose implementation already exists in the accepted native module (retained EXE actions / legacy cheat owners).
enum WeaponHostRow { WeaponGiveAll,WeaponMaxAmmo,WeaponInfiniteClip,WeaponInfiniteAmmo,WeaponExplosiveMelee,WeaponExplosiveAmmo,WeaponFlaming };
typedef void(*WeaponHostRowFn)(void*context,Transport&m,WeaponHostRow row);
namespace weapon_files {
struct Writer:pugi::xml_writer{
 HANDLE file;bool good;size_t total;explicit Writer(HANDLE handle):file(handle),good(true),total(0){}
 void write(const void*data,size_t size){const char*bytes=static_cast<const char*>(data);
  if(size>1024*1024-total){good=false;return;}total+=size;
  while(good&&size){const DWORD n=static_cast<DWORD>(size>0x7fffffffu?0x7fffffffu:size);DWORD wrote=0;
   good=WriteFile(file,bytes,n,&wrote,NULL)!=0&&wrote==n;bytes+=wrote;size-=wrote;}}
};
// Exclusive temporary + flush + atomic replace (same contract as ModelFavourites::save).
inline bool save(pugi::xml_document&doc,const std::string&path){
 static unsigned counter=0;char temporary[600];if(path.size()>500||sprintf_s(temporary,"%s.tmp-%lu-%u",path.c_str(),GetCurrentProcessId(),++counter)<0)return false;
 HANDLE file=CreateFileA(temporary,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(file==INVALID_HANDLE_VALUE)return false;
 Writer writer(file);doc.save(writer,"  ",pugi::format_default,pugi::encoding_utf8);
 const bool flushed=writer.good&&FlushFileBuffers(file)!=0;const bool closed=CloseHandle(file)!=0;
 const bool committed=flushed&&closed&&MoveFileExA(temporary,path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
 if(!committed)DeleteFileA(temporary);return committed;
}
inline bool load(pugi::xml_document&doc,const std::string&path,const char*root){
 WIN32_FILE_ATTRIBUTE_DATA data={};if(!GetFileAttributesExA(path.c_str(),GetFileExInfoStandard,&data))return false;
 if((data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||data.nFileSizeHigh||data.nFileSizeLow>1024*1024)return false;
 return doc.load_file(path.c_str(),pugi::parse_default|pugi::parse_declaration)&&!strcmp(doc.document_element().name(),root);
}
inline bool hash(const char*text,uint32_t&value){if(!text||!*text||*text=='-')return false;char*end=NULL;const unsigned long n=strtoul(text,&end,0);
 if(!end||*end)return false;value=static_cast<uint32_t>(n);return true;}
}
class WeaponOptions {
 WeaponPageIds ids_;TeleportActions*teleport_;float*worldSpeed_;
 enum Toggle { InfiniteParachutes,Tenfold,BulletTime,Triggerbot,RapidFire,SoulSwitch,TeleportGun,RevivalGun,RemovalGun,Laser,ToggleCount };
 bool on_[ToggleCount],was_[ToggleCount];float damage_;int bulletTimeUntil_;
 int category_,weapon_;bool held_;uint32_t heldHash_;
 int laserR_,laserG_,laserB_,laserA_;
 std::string root_,favouritesPath_,dir_,loadout_;
 int soulPed_,soulAt_;
 struct Stored {uint32_t weapon;std::vector<std::pair<uint32_t,int> >components;int tint;};
 static const uint32_t COMBATPISTOL=0x5EF9FEC4u,HEAVYPISTOL=0xD205520Eu,SNSPISTOL=0xBFD21232u,STUNGUN=0x3656C8C1u,UNARMED=0xA2719263u,PARACHUTE=0xFBAB5776u;
 static const uint32_t GROUP_MELEE=0xD49321D4u,GROUP_EXTINGUISHER=0xFDBF656Cu,GROUP_SPILLABLE=0x5F1BE07Cu,GROUP_THROWABLE=0x5C4C5883u,GROUP_SNIPER=0xB7BBD827u;
 static unsigned weaponCount(){return sizeof(weapons::WEAPONS)/sizeof(weapons::WEAPONS[0]);}
 static const char*label(uint32_t hash){
  size_t lo=0,hi=sizeof(weapons::LABELS)/sizeof(weapons::LABELS[0]);
  while(lo<hi){const size_t mid=(lo+hi)/2;if(weapons::LABELS[mid].hash<hash)lo=mid+1;else hi=mid;}
  return lo<sizeof(weapons::LABELS)/sizeof(weapons::LABELS[0])&&weapons::LABELS[lo].hash==hash?weapons::LABELS[lo].name:"Unknown";
 }
 static const weapons::Weapon*find(uint32_t hash){for(unsigned i=0;i<weaponCount();++i)if(weapons::WEAPONS[i].hash==hash)return &weapons::WEAPONS[i];return NULL;}
 static unsigned group(uint32_t weapon){return static_cast<unsigned>(sn::GET_WEAPONTYPE_GROUP(static_cast<int>(weapon)));}
 static uint32_t current(int ped){int w=0;sn::GET_CURRENT_PED_WEAPON(ped,&w,true);return static_cast<uint32_t>(w);}
 static void fillAmmo(int ped,uint32_t w){int maximum=0;sn::GET_MAX_AMMO(ped,static_cast<int>(w),&maximum);
  sn::SET_AMMO_IN_CLIP(ped,static_cast<int>(w),sn::GET_MAX_AMMO_IN_CLIP(ped,static_cast<int>(w),false));sn::SET_PED_AMMO(ped,static_cast<int>(w),maximum,false);}
 static void giveParachute(int ped){const int state=sn::GET_PED_PARACHUTE_STATE(ped);
  if(state==-1||state==-3||state==3)sn::GIVE_DELAYED_WEAPON_TO_PED(ped,static_cast<int>(PARACHUTE),1,false);}
 // GTAped::StoreWeaponsInArray / GiveWeaponsFromArray (catalogue order, components with liveries, tint, parachute).
 static void store(int ped,std::vector<Stored>&out){out.clear();
  for(unsigned i=0;i<weaponCount();++i){const weapons::Weapon&w=weapons::WEAPONS[i];const int wh=static_cast<int>(w.hash);
   if(!sn::HAS_PED_GOT_WEAPON(ped,wh,false))continue;Stored s;s.weapon=w.hash;
   for(unsigned c=0;c<w.count;++c){const int ch=static_cast<int>(weapons::COMPONENTS[w.first+c].hash);
    if(sn::HAS_PED_GOT_WEAPON_COMPONENT(ped,wh,ch))s.components.push_back(std::make_pair(static_cast<uint32_t>(ch),sn::GET_PED_WEAPON_COMPONENT_TINT_INDEX(ped,wh,ch)));}
   s.tint=sn::GET_PED_WEAPON_TINT_INDEX(ped,wh);out.push_back(s);}
  if(sn::HAS_PED_GOT_WEAPON(ped,static_cast<int>(PARACHUTE),false)){Stored s;s.weapon=PARACHUTE;s.tint=0;out.push_back(s);}
 }
 static void give(int ped,const std::vector<Stored>&in){
  for(size_t i=0;i<in.size();++i){const Stored&s=in[i];if(!s.weapon)continue;const int wh=static_cast<int>(s.weapon);
   sn::GIVE_WEAPON_TO_PED(ped,wh,1,true,false);
   for(size_t c=0;c<s.components.size();++c){if(!s.components[c].first)continue;const int ch=static_cast<int>(s.components[c].first);
    sn::GIVE_WEAPON_COMPONENT_TO_PED(ped,wh,ch);if(s.components[c].second!=-1)sn::SET_PED_WEAPON_COMPONENT_TINT_INDEX(ped,wh,ch,s.components[c].second);}
   if(s.weapon!=PARACHUTE){int ammo=0;sn::GET_MAX_AMMO(ped,wh,&ammo);sn::SET_PED_AMMO(ped,wh,ammo,false);
    sn::SET_AMMO_IN_CLIP(ped,wh,sn::GET_MAX_AMMO_IN_CLIP(ped,wh,true));sn::SET_PED_WEAPON_TINT_INDEX(ped,wh,s.tint);}}
 }
 // VR aim: muzzle position and barrel (X) axis of the held weapon object.
 bool aim(int ped,tw::V3&origin,tw::V3&direction)const{
  const int gun=sn::GET_CURRENT_PED_WEAPON_ENTITY_INDEX(ped,0);if(!tw::exists(gun))return false;
  const tw::V3 base=tw::position(gun);const int muzzle=sn::GET_ENTITY_BONE_INDEX_BY_NAME(gun,"gun_muzzle");
  origin=muzzle>=0?sn::GET_WORLD_POSITION_OF_ENTITY_BONE(gun,muzzle):base;
  direction=tw::normal(tw::sub(tw::offset(gun,1.f,0,0),base));return tw::length(direction)>.5f;
 }
 // RaycastResult::Raycast(source,direction,distance,Everything,ignore) with the synchronous LOS probe.
 static bool ray(const tw::V3&from,const tw::V3&direction,float distance,int ignore,tw::V3&hit,int&entity){
  const tw::V3 to=tw::add(from,tw::mul(direction,distance));const int test=sn::START_EXPENSIVE_SYNCHRONOUS_SHAPE_TEST_LOS_PROBE(from,to,-1,ignore,7);
  int any=0;tw::V3 normal=tw::v3(0,0,0);hit=to;entity=0;
  if(sn::GET_SHAPE_TEST_RESULT(test,&any,&hit,&normal,&entity)!=2||!any){hit=to;entity=0;return false;}
  if(!tw::exists(entity))entity=0;return true;
 }
 int aimedEntity(int ped)const{tw::V3 o,d,h;int e=0;if(!aim(ped,o,d))return 0;ray(o,d,300.f,ped,h,e);return e;}
 void shoot(const tw::V3&from,const tw::V3&to,int ped,uint32_t weapon,float speed){
  sn::SHOOT_SINGLE_BULLET_BETWEEN_COORDS(from,to,5,true,static_cast<int>(weapon),ped,true,false,speed);}
 static bool toggleRow(Transport&m,const char*label,bool&value,const char*help){const bool c=m.add(label,ToggleRow,value?"ON":"OFF");if(help)m.description(help);
  if(c&&m.event()==Accept){value=!value;return true;}return false;}
 bool tick(Transport&m,const std::string&label,bool state,Icon on=TickIcon,Icon off=NoIcon){const bool c=m.add(label,ActionRow,"");m.icons(state,on,off);return c&&m.event()==Accept;}
 void applyDamage(){const int p=tw::player();sn::SET_PLAYER_WEAPON_DAMAGE_MODIFIER(p,damage_);sn::SET_PLAYER_MELEE_WEAPON_DAMAGE_MODIFIER(p,damage_,true);}
 uint32_t selected(int ped)const{
  if(held_)return current(ped);
  if(category_==9){std::vector<std::pair<uint32_t,std::string> >f=favourites();return weapon_>=0&&static_cast<size_t>(weapon_)<f.size()?f[weapon_].first:0;}
  unsigned n=0;for(unsigned i=0;i<weaponCount();++i)if(weapons::WEAPONS[i].category==static_cast<unsigned>(category_)&&n++==static_cast<unsigned>(weapon_))return weapons::WEAPONS[i].hash;
  return 0;
 }
 // Favourites (FavouriteWeapons.xml, Menyoo schema Weapon hash/customName).
 std::vector<std::pair<uint32_t,std::string> >favourites()const{
  std::vector<std::pair<uint32_t,std::string> >out;pugi::xml_document doc;if(favouritesPath_.empty()||!weapon_files::load(doc,favouritesPath_,"FavouriteWeapons"))return out;
  for(pugi::xml_node n=doc.document_element().child("Weapon");n&&out.size()<512;n=n.next_sibling("Weapon")){uint32_t h=0;
   if(weapon_files::hash(n.attribute("hash").value(),h))out.push_back(std::make_pair(h,std::string(n.attribute("customName").value())));}
  return out;
 }
 bool isFavourite(uint32_t hash)const{std::vector<std::pair<uint32_t,std::string> >f=favourites();for(size_t i=0;i<f.size();++i)if(f[i].first==hash)return true;return false;}
 bool setFavourite(uint32_t hash,bool add){
  if(favouritesPath_.empty()||!hash)return false;pugi::xml_document doc;
  if(!weapon_files::load(doc,favouritesPath_,"FavouriteWeapons")){
   if(GetFileAttributesA(favouritesPath_.c_str())!=INVALID_FILE_ATTRIBUTES)return false; // never overwrite an unreadable/unrelated file
   doc.reset();pugi::xml_node d=doc.append_child(pugi::node_declaration);d.append_attribute("version")="1.0";d.append_attribute("encoding")="UTF-8";doc.append_child("FavouriteWeapons");}
  char hex[16];sprintf_s(hex,"0x%08X",hash);pugi::xml_node root=doc.document_element();
  for(pugi::xml_node n=root.child("Weapon");n;){pugi::xml_node next=n.next_sibling("Weapon");uint32_t h=0;if(weapon_files::hash(n.attribute("hash").value(),h)&&h==hash)root.remove_child(n);n=next;}
  if(add){pugi::xml_node n=root.append_child("Weapon");n.append_attribute("hash")=hex;n.append_attribute("customName")=label(hash);}
  return weapon_files::save(doc,favouritesPath_);
 }
 // Loadouts (Loadout/Weapon hash,tint/Components/Component hash,livery), folder-relative paths under root_.
 bool createLoadout(int ped,const std::string&path){
  pugi::xml_document doc;pugi::xml_node d=doc.append_child(pugi::node_declaration);d.append_attribute("version")="1.0";d.append_attribute("encoding")="UTF-8";
  pugi::xml_node root=doc.append_child("Loadout");root.append_attribute("menyoo_ver")="GTAVXR";std::vector<Stored>w;store(ped,w);char hex[16];
  for(size_t i=0;i<w.size();++i){pugi::xml_node n=root.append_child("Weapon");sprintf_s(hex,"0x%X",w[i].weapon);n.append_attribute("hash")=hex;n.append_attribute("tint")=w[i].tint;
   pugi::xml_node cs=n.append_child("Components");
   for(size_t c=0;c<w[i].components.size();++c){pugi::xml_node cn=cs.append_child("Component");sprintf_s(hex,"0x%X",w[i].components[c].first);cn.append_attribute("hash")=hex;
    sprintf_s(hex,"0x%X",static_cast<unsigned>(w[i].components[c].second));cn.append_attribute("livery")=hex;}}
  return weapon_files::save(doc,path);
 }
 bool applyLoadout(int ped,const std::string&path){
  pugi::xml_document doc;if(!weapon_files::load(doc,path,"Loadout"))return false;std::vector<Stored>w;
  for(pugi::xml_node n=doc.document_element().child("Weapon");n&&w.size()<256;n=n.next_sibling("Weapon")){Stored s;s.weapon=0;weapon_files::hash(n.attribute("hash").value(),s.weapon);s.tint=n.attribute("tint").as_int();
   for(pugi::xml_node c=n.child("Components").first_child();c&&s.components.size()<64;c=c.next_sibling()){uint32_t h=0;pugi::xml_attribute ha=c.attribute("hash"),la=c.attribute("livery");
    if(ha)weapon_files::hash(ha.value(),h);else weapon_files::hash(c.text().as_string(),h);s.components.push_back(std::make_pair(h,la?static_cast<int>(la.as_uint()):-1));}
   w.push_back(s);}
  sn::REMOVE_ALL_PED_WEAPONS(ped,true);give(ped,w);return true;
 }
 std::string folder()const{return dir_.empty()?root_:dir_;}
 static bool isDirectory(const std::string&p){const DWORD a=GetFileAttributesA(p.c_str());return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY);}
 void listFolder(std::vector<std::string>&folders,std::vector<std::string>&files)const{
  folders.clear();files.clear();WIN32_FIND_DATAA data;HANDLE h=FindFirstFileA((folder()+"\\*").c_str(),&data);if(h==INVALID_HANDLE_VALUE)return;
  do{const std::string n=data.cFileName;if(n.empty()||n[0]=='.'||n[0]==',')continue;
   if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)folders.push_back(n);
   else if(n.size()>4&&_stricmp(n.c_str()+n.size()-4,".xml")==0)files.push_back(n.substr(0,n.size()-4));}while(FindNextFileA(h,&data)&&folders.size()+files.size()<1024);
  FindClose(h);std::sort(folders.begin(),folders.end());std::sort(files.begin(),files.end());
 }
 std::string nextLoadoutName()const{std::vector<std::string>f,x;listFolder(f,x);char n[32];
  for(int i=1;i<10000;++i){sprintf_s(n,"Loadout %03d",i);if(std::find(x.begin(),x.end(),std::string(n))==x.end())return n;}return "Loadout";}
 // Menyoo SetBecomePed (soul switch): weapons, vehicle seat and CHANGE_PLAYER_PED.
 void becomePed(int target){
  const int old=tw::ped();std::vector<Stored>backup;store(old,backup);int vehicle=0,seat=-2;
  if(sn::IS_PED_IN_ANY_VEHICLE(target,false)){vehicle=sn::GET_VEHICLE_PED_IS_IN(target,false);const int maximum=sn::GET_VEHICLE_MAX_NUMBER_OF_PASSENGERS(vehicle);
   for(int i=-1;i<=maximum;++i)if(sn::GET_PED_IN_VEHICLE_SEAT(vehicle,i,false)==target){seat=i;break;}}
  if(sn::IS_PED_A_PLAYER(target))return;
  sn::SET_ENTITY_INVINCIBLE(old,false);sn::SET_PED_DIES_IN_WATER(old,true);sn::SET_ENTITY_PROOFS(old,false,false,false,false,false,false,false,false);
  sn::CHANGE_PLAYER_PED(tw::player(),target,true,true);const int now=tw::ped();
  if(vehicle&&tw::exists(vehicle)&&seat>=-1)sn::SET_PED_INTO_VEHICLE(now,vehicle,seat);
  give(now,backup);int h=old;if(tw::exists(h))sn::SET_PED_AS_NO_LONGER_NEEDED(&h);
 }
 void soulSwitchTick(int ped,uint32_t weapon,int now){
  if(soulAt_){if(now-soulAt_>=64){const int p=tw::ped();sn::SET_CURRENT_PED_WEAPON(p,static_cast<int>(COMBATPISTOL),true);sn::SET_PED_CURRENT_WEAPON_VISIBLE(p,true,true,true,false);soulAt_=0;}return;}
  if(weapon!=COMBATPISTOL)return;
  sn::DISABLE_CONTROL_ACTION(0,24,true);sn::DISABLE_CONTROL_ACTION(2,257,true);sn::DISABLE_CONTROL_ACTION(2,69,true);sn::DISABLE_CONTROL_ACTION(2,70,true);
  const int target=aimedEntity(ped);
  if(!target||!sn::IS_ENTITY_A_PED(target)||sn::IS_ENTITY_DEAD(target,false))return;
  if(!(sn::IS_DISABLED_CONTROL_JUST_PRESSED(0,24)||(sn::IS_PED_IN_ANY_VEHICLE(ped,false)&&sn::IS_DISABLED_CONTROL_JUST_PRESSED(2,69))))return;
  if(sn::IS_SPECIAL_ABILITY_ACTIVE(tw::player(),0))sn::SPECIAL_ABILITY_DEACTIVATE_FAST(tw::player(),0);
  sn::PLAY_SOUND_FRONTEND(-1,"Knuckle_Crack_Hard_Cel","MP_SNACKS_SOUNDSET",false);sn::ANIMPOSTFX_PLAY("MinigameEndNeutral",0,false);
  becomePed(target);soulAt_=now?now:1;
 }
 void removalTick(int ped,uint32_t weapon){
  if(weapon!=SNSPISTOL||!sn::IS_PED_SHOOTING(ped))return;int e=aimedEntity(ped);
  if(!e||e==ped||(sn::IS_ENTITY_A_PED(e)&&sn::IS_PED_A_PLAYER(e)))return;
  if(sn::IS_ENTITY_A_PED(e)&&sn::IS_PED_IN_ANY_VEHICLE(e,false))sn::CLEAR_PED_TASKS_IMMEDIATELY(e);
  sn::SET_ENTITY_AS_MISSION_ENTITY(e,true,true);if(sn::DOES_ENTITY_BELONG_TO_THIS_SCRIPT(e,false))sn::DELETE_ENTITY(&e);
 }
 void revivalTick(int ped,uint32_t weapon){
  if(weapon!=STUNGUN||!sn::IS_PED_SHOOTING(ped))return;const int p=aimedEntity(ped);
  if(!p||!sn::IS_ENTITY_A_PED(p)||!sn::IS_ENTITY_DEAD(p,false))return;
  sn::SET_ENTITY_HEALTH(p,200,0);sn::RESURRECT_PED(p);sn::REVIVE_INJURED_PED(p);sn::SET_PED_MAX_HEALTH(p,400);sn::SET_ENTITY_HEALTH(p,200,0);
  sn::SET_PED_GENERATES_DEAD_BODY_EVENTS(p,false);sn::SET_PED_CONFIG_FLAG(p,166,false);sn::SET_PED_CONFIG_FLAG(p,187,false);sn::CLEAR_PED_TASKS_IMMEDIATELY(p);
  const char*dict="get_up@directional@movement@from_knees@standard";sn::REQUEST_ANIM_DICT(dict);int sequence=0;sn::OPEN_SEQUENCE_TASK(&sequence);
  sn::TASK_PLAY_ANIM(0,dict,rand()%3==1?"getup_r_90":"getup_l_90",8.f,-8.f,-1,32,0.f,false,false,false);sn::TASK_WANDER_STANDARD(0,10.f,10);
  sn::CLOSE_SEQUENCE_TASK(sequence);sn::TASK_PERFORM_SEQUENCE(p,sequence);sn::CLEAR_SEQUENCE_TASK(&sequence);
 }
 void teleportTick(int ped,uint32_t weapon){
  if(weapon!=HEAVYPISTOL||!sn::IS_PED_SHOOTING(ped)||!teleport_)return;tw::V3 o,d,hit;int e=0;if(!aim(ped,o,d)||!ray(o,d,15000.f,ped,hit,e))return;
  const int subject=tw::vehicle()?tw::vehicle():ped;tw::V3 target=hit;
  if(e&&(sn::IS_ENTITY_A_VEHICLE(e)||!sn::IS_ENTITY_A_MISSION_ENTITY(e))){tw::V3 hf,hb,sf,sb;tw::extents(tw::model(e),hf,hb);tw::extents(tw::model(subject),sf,sb);
   target=tw::add(tw::position(e),tw::v3(0,0,hb.z+sf.z));}
  if(target.x==0&&target.y==0&&target.z==0)return;
  if(teleport_->coordinates(target.x,target.y,target.z)){sn::PLAY_SOUND_FRONTEND(-1,"Knuckle_Crack_Hard_Cel","MP_SNACKS_SOUNDSET",false);sn::ANIMPOSTFX_PLAY("ExplosionJosh3",0,false);}
  else tw::notify("Teleport rejected; native state was preserved.");
 }
 void tenfoldTick(int ped,uint32_t weapon){
  if(group(weapon)==GROUP_MELEE)return;tw::V3 o,d,hit;int e=0;if(!aim(ped,o,d))return;
  const float range=sn::GET_MAX_RANGE_OF_CURRENT_PED_WEAPON(ped);ray(o,d,range>1.f?range:100.f,ped,hit,e);
  static const float spread[6][3]={{-2,2,2},{-1.5f,1,1},{-1,0,0},{1,0,0},{1.5f,1,1},{2,2,2}};
  sn::CLEAR_AREA_OF_PROJECTILES(o,6.f,0);for(int i=0;i<6;++i)shoot(o,tw::add(hit,tw::v3(spread[i][0],spread[i][1],spread[i][2])),ped,weapon,-1.f);
 }
 void rapidTick(int ped,uint32_t weapon){
  if(group(weapon)==GROUP_MELEE)return;sn::DISABLE_CONTROL_ACTION(0,24,true);sn::DISABLE_CONTROL_ACTION(2,257,true);
  if(!sn::IS_DISABLED_CONTROL_PRESSED(0,24))return;tw::V3 o,d;if(!aim(ped,o,d))return;
  const tw::V3 launch=tw::add(o,tw::mul(d,.4f)),target=tw::add(o,tw::mul(d,200.f));sn::CLEAR_AREA_OF_PROJECTILES(launch,6.f,0);
  shoot(launch,target,ped,weapon,24000.f);shoot(launch,target,ped,weapon,24000.f);
 }
 void triggerTick(int ped,uint32_t weapon){
  const int t=aimedEntity(ped);if(!t||!sn::IS_ENTITY_A_PED(t)||sn::IS_ENTITY_DEAD(t,false)||!sn::IS_WEAPON_VALID(static_cast<int>(weapon)))return;
  sn::SET_PED_SHOOTS_AT_COORD(ped,sn::GET_PED_BONE_COORDS(t,31086,tw::v3(0,0,0)),false);
 }
 void laserTick(int ped,uint32_t weapon){
  const unsigned g=group(weapon);if(g==GROUP_EXTINGUISHER||g==GROUP_MELEE||g==GROUP_SPILLABLE||g==GROUP_THROWABLE||sn::IS_PED_RELOADING(ped))return;
  tw::V3 o,d,hit;int e=0;if(!aim(ped,o,d))return;
  if(ray(o,d,300.f,ped,hit,e))hit=tw::add(hit,tw::v3(0,0,-.07f));else hit=tw::add(o,tw::mul(d,1000.f));
  const float dist=tw::distance(tw::position(ped),hit);const float dot=dist<7.f?.009f:.016f;
  sn::DRAW_LINE(o,hit,laserR_,laserG_,laserB_,laserA_);
  sn::DRAW_MARKER(28,hit,tw::v3(0,0,0),tw::v3(0,0,0),tw::v3(dot,dot,dot),laserR_,laserG_,laserB_,laserA_,false,false,2,false,NULL,NULL,false);
 }
public:
 WeaponOptions():teleport_(NULL),worldSpeed_(NULL){memset(&ids_,0,sizeof(ids_));resetState();}
 void resetState(){memset(on_,0,sizeof(on_));memset(was_,0,sizeof(was_));damage_=1.f;bulletTimeUntil_=0;category_=0;weapon_=0;held_=false;heldHash_=0;
  laserR_=255;laserG_=laserB_=0;laserA_=255;dir_.clear();loadout_.clear();soulPed_=soulAt_=0;}
 bool bind(const WeaponPageIds&ids,ResetLedger&ledger,TeleportActions&teleport,float&worldSpeed){
  ids_=ids;teleport_=&teleport;worldSpeed_=&worldSpeed;resetState();
  for(unsigned i=0;i<ToggleCount;++i)if(!ledger.bindToggle(on_[i],NULL))return false;return true;
 }
 // Menyoo Pathff::Main\FavouriteWeapons.xml and \menyooStuff\WeaponsLoadout, placed next to the Trainer settings.
 void configure(const char*settings){
  root_.clear();favouritesPath_.clear();dir_.clear();if(!settings)return;std::string s=settings;const size_t slash=s.find_last_of("\\/");if(slash==std::string::npos)return;
  const std::string base=s.substr(0,slash);favouritesPath_=base+"\\FavouriteWeapons.xml";root_=base+"\\WeaponsLoadout";
  if(!isDirectory(root_))CreateDirectoryA(root_.c_str(),NULL);
 }
 bool page(Transport&m,WeaponHostRowFn host,void*context){
  m.setTitle("Weapon Options");const int ped=tw::ped();
  m.add("Individual Weapons & Customisation",ActionRow,"",ids_.indivs);
  m.add("Loadouts",ActionRow,"",ids_.loadouts);
  host(context,m,WeaponGiveAll);host(context,m,WeaponMaxAmmo);
  if(m.add("Remove All Weapons",ActionRow,"")&&m.event()==Accept)sn::REMOVE_ALL_PED_WEAPONS(ped,true);
  toggleRow(m,"Infinite Parachutes",on_[InfiniteParachutes],"Gives you a new parachute whenever you don't have one.");
  host(context,m,WeaponInfiniteClip);host(context,m,WeaponInfiniteAmmo);
  host(context,m,WeaponExplosiveMelee);host(context,m,WeaponExplosiveAmmo);
  toggleRow(m,"Tenfold Bullets",on_[Tenfold],"Each shot fires a spread of extra bullets.");
  host(context,m,WeaponFlaming);
  char t[32];sprintf_s(t,"%.2fx",damage_/.72f);
  if(m.add("Weapon Damage",ChoiceRow,t)){
   if(m.event()==Accept){damage_=(damage_==1.f?100.f:1.f)*.72f;applyDamage();}
   else if(m.event()==Right){if(damage_/.72f<100.f)damage_+=.1f*.72f;applyDamage();}
   else if(m.event()==Left){if(damage_/.72f>-100.f)damage_-=.1f*.72f;applyDamage();}}
  m.description("Multiplies your weapon and melee damage. 1.0 is normal.");
  if(toggleRow(m,"Bullet Time",on_[BulletTime],"Slows time briefly whenever you shoot.")&&!on_[BulletTime]&&worldSpeed_)sn::SET_TIME_SCALE(*worldSpeed_);
  toggleRow(m,"Triggerbot",on_[Triggerbot],"Shoots automatically when aiming at a living ped.");
  if(toggleRow(m,"Rapid Fire",on_[RapidFire],"Fires as fast as possible while holding the trigger.")&&on_[RapidFire])tw::notify("~b~Note:~s~ You cannot use other Menyoo weapon mods with this on.");
  if(toggleRow(m,"Soul-Switch Gun (SP)",on_[SoulSwitch],"Aim at a ped with the Combat Pistol and shoot to take control of them.")&&on_[SoulSwitch])tw::notify("Shoot ~b~scrubs~s~ with the ~b~combat pistol~s~ for hax!");
  if(toggleRow(m,"Teleport Gun",on_[TeleportGun],"Shoot with the Heavy Pistol to teleport where the bullet lands.")&&on_[TeleportGun])tw::notify("Use the ~b~Heavy pistol~s~ to teleport to places.");
  if(toggleRow(m,"Ped Revival Gun",on_[RevivalGun],"Shoot a dead ped with the Stun Gun to bring them back to life.")&&on_[RevivalGun])tw::notify("Shoot ~b~dead people~s~ with the ~b~stun gun~s~ to bring them back from the other side.");
  if(toggleRow(m,"Entity Removal Gun",on_[RemovalGun],"Shoot anything with the SNS Pistol to delete it.")&&on_[RemovalGun])tw::notify("Shoot ~b~anything*~s~ with the ~b~SNS pistol~s~ to delete it.");
  m.add("Laser Sight",ActionRow,"",ids_.laser);
  return true;
 }
 bool indivsPage(Transport&m){
  m.setTitle("Individual Weapons");const int ped=tw::ped();
  if(m.add("Currently Held Weapon",ActionRow,"",ids_.item)&&m.event()==Accept){held_=true;heldHash_=current(ped);}
  if(current(ped)==UNARMED||!find(current(ped))){m.rows.back().enabled=false;m.description("Hold a catalogued weapon to customise it.");}
  m.add("Parachutes",ActionRow,"",ids_.parachute);
  for(int i=0;i<9;++i)if(m.add(weapons::CATEGORY_NAMES[i],ActionRow,"",ids_.category)&&m.event()==Accept){category_=i;held_=false;}
  if(m.add("Favourites",ActionRow,"",ids_.favourites)&&m.event()==Accept){category_=9;held_=false;}
  return true;
 }
 bool categoryPage(Transport&m){
  m.setTitle(category_>=0&&category_<10?weapons::CATEGORY_NAMES[category_]:"Weapons");const int ped=tw::ped();int n=0;
  for(unsigned i=0;i<weaponCount();++i){const weapons::Weapon&w=weapons::WEAPONS[i];if(w.category!=static_cast<unsigned>(category_))continue;
   if(m.add(label(w.hash),ActionRow,"",ids_.item)&&m.event()==Accept){weapon_=n;held_=false;}
   m.icons(sn::HAS_PED_GOT_WEAPON(ped,static_cast<int>(w.hash),false),WeaponIcon,NoIcon);
   m.description(isFavourite(w.hash)?"Y: remove from favourites.":"Y: add to favourites.");++n;}
  return true;
 }
 // Y (Favourite event) on a weapon row of the category page: Menyoo menu_action add/remove favourite.
 void toggleFavourite(unsigned row){
  unsigned n=0;for(unsigned i=0;i<weaponCount();++i){const weapons::Weapon&w=weapons::WEAPONS[i];if(w.category!=static_cast<unsigned>(category_))continue;
   if(n++==row){const bool add=!isFavourite(w.hash);tw::notify(setFavourite(w.hash,add)?(add?"Weapon added.":"Weapon removed."):"Error: Unable to save favourites.");return;}}
 }
 bool favouritesPage(Transport&m){
  m.setTitle("Favourites");const int ped=tw::ped();const uint32_t held=current(ped);
  if(held!=UNARMED&&held){const bool fav=isFavourite(held);
   if(tick(m,"Currently Held Weapon",fav,BoxTickIcon,BoxBlankIcon)){if(setFavourite(held,!fav))tw::notify(fav?"Weapon removed.":"Weapon added.");else tw::notify("Error: Unable to save favourites.");}
   m.description("Adds the weapon in your hand using its installed name.");}
  std::vector<std::pair<uint32_t,std::string> >f=favourites();
  if(!f.empty())m.add("---Added Weapons---",BreakRow,"");
  for(size_t i=0;i<f.size();++i){if(m.add(f[i].second.empty()?label(f[i].first):f[i].second,ActionRow,"",ids_.item)&&m.event()==Accept){category_=9;weapon_=static_cast<int>(i);held_=false;}
   m.description("Y: remove from favourites.");}
  return true;
 }
 void removeFavouriteRow(unsigned row,unsigned offset){std::vector<std::pair<uint32_t,std::string> >f=favourites();
  if(row>=offset&&row-offset<f.size())tw::notify(setFavourite(f[row-offset].first,false)?"Weapon removed.":"Error: Unable to save favourites.");}
 unsigned favouriteRowOffset()const{const uint32_t held=current(tw::ped());return (held!=UNARMED&&held?1u:0u)+1u;}
 bool category9()const{return category_==9;}
 bool itemPage(Transport&m){
  const int ped=tw::ped();const uint32_t w=selected(ped);if(!w){m.setTitle("Weapons");m.add("Unavailable",ActionRow,"");m.rows.back().enabled=false;return true;}
  m.setTitle(label(w));const int wh=static_cast<int>(w);
  if(tick(m,"Equip Weapon",current(ped)==w)){if(!sn::HAS_PED_GOT_WEAPON(ped,wh,false)){sn::GIVE_DELAYED_WEAPON_TO_PED(ped,wh,1000,false);fillAmmo(ped,w);}
   sn::SET_CURRENT_PED_WEAPON(ped,wh,true);}
  if(m.add("Fill Ammo",ActionRow,"")&&m.event()==Accept)fillAmmo(ped,w);m.description("Sets this weapon's ammo to max.");
  if(m.add("Empty Ammo",ActionRow,"")&&m.event()==Accept){sn::SET_AMMO_IN_CLIP(ped,wh,-1);sn::SET_PED_AMMO(ped,wh,-1,false);}m.description("Sets this weapon's ammo to zero.");
  if(m.add("Remove Weapon",ActionRow,"")&&m.event()==Accept&&sn::HAS_PED_GOT_WEAPON(ped,wh,false))sn::REMOVE_WEAPON_FROM_PED(ped,wh);
  const weapons::Weapon*info=find(w);if(info&&(info->count||info->tints))m.add("Attachments & Tints",ActionRow,"",ids_.mods);
  return true;
 }
 bool modsPage(Transport&m){
  const int ped=tw::ped();const uint32_t w=selected(ped);const weapons::Weapon*info=find(w);m.setTitle("Attachments & Tints");if(!info)return true;
  const int wh=static_cast<int>(w);const int tint=sn::GET_PED_WEAPON_TINT_INDEX(ped,wh);
  if(m.event()==NoEvent&&!sn::HAS_PED_GOT_WEAPON(ped,wh,false))sn::GIVE_WEAPON_TO_PED(ped,wh,9999,false,true);
  if(m.event()==NoEvent&&current(ped)!=w)sn::SET_CURRENT_PED_WEAPON(ped,wh,true);
  for(unsigned c=0;c<info->count;++c){const weapons::Component&comp=weapons::COMPONENTS[info->first+c];const int ch=static_cast<int>(comp.hash);
   if(!sn::DOES_WEAPON_TAKE_WEAPON_COMPONENT(wh,ch))continue;const bool has=sn::HAS_PED_GOT_WEAPON_COMPONENT(ped,wh,ch);
   std::string upper=comp.name;for(size_t k=0;k<upper.size();++k)upper[k]=static_cast<char>(toupper(static_cast<unsigned char>(upper[k])));
   if(has&&upper.find("CAMO")!=std::string::npos){int livery=sn::GET_PED_WEAPON_COMPONENT_TINT_INDEX(ped,wh,ch);char gxt[32];sprintf_s(gxt,"WCT_C_TINT_%d",livery);
    if(m.add(comp.name,ChoiceRow,tw::text(gxt,gxt))){
     if(m.event()==Right&&livery<31)sn::SET_PED_WEAPON_COMPONENT_TINT_INDEX(ped,wh,ch,livery+1);
     else if(m.event()==Left&&livery>0)sn::SET_PED_WEAPON_COMPONENT_TINT_INDEX(ped,wh,ch,livery-1);
     else if(m.event()==Accept)sn::SET_PED_WEAPON_COMPONENT_TINT_INDEX(ped,wh,ch,livery);}}
   else if(tick(m,comp.name,has,WeaponIcon)){if(has)sn::REMOVE_WEAPON_COMPONENT_FROM_PED(ped,wh,ch);else sn::GIVE_WEAPON_COMPONENT_TO_PED(ped,wh,ch);}}
  if(info->tints){if(info->count)m.add("---Tints---",BreakRow,"");const unsigned n=info->tints==2?32u:8u;
   for(unsigned i=0;i<n;++i)if(tick(m,info->tints==2?weapons::TINTS_MK2[i]:weapons::TINTS[i],tint==static_cast<int>(i),WeaponIcon))sn::SET_PED_WEAPON_TINT_INDEX(ped,wh,static_cast<int>(i));}
  return true;
 }
 bool parachutePage(Transport&m){
  const int ped=tw::ped(),player=tw::player();int tint=0;sn::GET_PLAYER_PARACHUTE_TINT_INDEX(player,&tint);
  const bool has=sn::HAS_PED_GOT_WEAPON(ped,static_cast<int>(PARACHUTE),false);m.setTitle("Parachute");
  if(tick(m,"Equip Primary",has))giveParachute(ped);
  if(tick(m,"Equip Secondary",has)){giveParachute(ped);sn::SET_PLAYER_HAS_RESERVE_PARACHUTE(player);}
  if(m.add("Remove",ActionRow,"")&&m.event()==Accept&&has)sn::REMOVE_WEAPON_FROM_PED(ped,static_cast<int>(PARACHUTE));
  int r=0,g=0,b=0;sn::GET_PLAYER_PARACHUTE_SMOKE_TRAIL_COLOR(player,&r,&g,&b);char rgb[32];sprintf_s(rgb,"%d, %d, %d",r,g,b);
  m.add("Set Smoke Colour",ActionRow,rgb,ids_.smoke);
  m.add("---Tints---",BreakRow,"");
  for(int i=0;i<14;++i)if(tick(m,weapons::CHUTE_TINTS[i],tint==i,WeaponIcon)){sn::SET_PLAYER_PARACHUTE_TINT_INDEX(player,i);
   if(sn::IS_DLC_PRESENT(static_cast<int>(0xC40B8B70u)))sn::SET_PLAYER_PARACHUTE_MODEL_OVERRIDE(player,0x73268708);
   else if(sn::IS_DLC_PRESENT(0x55292CC7))sn::SET_PLAYER_PARACHUTE_MODEL_OVERRIDE(player,static_cast<int>(0x815E52EBu));}
  return true;
 }
 // Menyoo MSPAINTS_RGB mode 7 (parachute smoke): RGB steppers and the neon colour presets.
 bool smokePage(Transport&m){
  const int player=tw::player();int c[3]={0,0,0};sn::GET_PLAYER_PARACHUTE_SMOKE_TRAIL_COLOR(player,&c[0],&c[1],&c[2]);m.setTitle("Set Colour");
  const char*n[3]={"Red","Green","Blue"};char t[16];
  for(int i=0;i<3;++i){sprintf_s(t,"%d",c[i]);if(m.add(n[i],NumberRow,t)&&(m.event()==Left||m.event()==Right)){c[i]=(c[i]+(m.event()==Right?5:251))%256;
    sn::SET_PLAYER_CAN_LEAVE_PARACHUTE_SMOKE_TRAIL(player,true);sn::SET_PLAYER_PARACHUTE_SMOKE_TRAIL_COLOR(player,c[0],c[1],c[2]);}}
  const customs::Preset*p=customs::NEON_PRESETS;const unsigned presetCount=sizeof(customs::NEON_PRESETS)/sizeof(customs::NEON_PRESETS[0]);m.add("---Presets---",BreakRow,"");
  for(unsigned i=0;i<presetCount;++i)if(tick(m,p[i].name,c[0]==p[i].r&&c[1]==p[i].g&&c[2]==p[i].b)){
   sn::SET_PLAYER_CAN_LEAVE_PARACHUTE_SMOKE_TRAIL(player,true);sn::SET_PLAYER_PARACHUTE_SMOKE_TRAIL_COLOR(player,p[i].r,p[i].g,p[i].b);}
  return true;
 }
 bool loadoutsPage(Transport&m){
  m.setTitle("Loadouts");const int ped=tw::ped();
  if(m.add("Save Current Loadout",ActionRow,"")&&m.event()==Accept){
   if(root_.empty())tw::notify("Error: Unable to save loadout.");
   else{const std::string name=nextLoadoutName();tw::notify(createLoadout(ped,folder()+"\\"+name+".xml")?"Loadout saved.":"Error: Unable to save loadout.");}}
  m.description("Saves all weapons, attachments and tints you're carrying to a new numbered file.");
  m.add("---Found Files---",BreakRow,"");
  if(m.add("..",ActionRow,"")&&m.event()==Accept&&!dir_.empty()&&dir_!=root_){const size_t s=dir_.find_last_of('\\');dir_=s==std::string::npos?root_:dir_.substr(0,s);
   if(dir_.size()<root_.size())dir_=root_;}
  std::vector<std::string>folders,files;listFolder(folders,files);
  for(size_t i=0;i<folders.size();++i)if(m.add(folders[i]+" >>>",ActionRow,"")&&m.event()==Accept)dir_=folder()+"\\"+folders[i];
  for(size_t i=0;i<files.size();++i){if(m.add(files[i],ActionRow,"",ids_.loadout)&&m.event()==Accept)loadout_=files[i];m.icons(true,TickIcon,NoIcon);}
  return true;
 }
 bool loadoutPage(Transport&m,bool&leave){
  leave=false;const std::string path=folder()+"\\"+loadout_+".xml";m.setTitle(loadout_);const int ped=tw::ped();
  if(m.add("Apply",ActionRow,"")&&m.event()==Accept)tw::notify(applyLoadout(ped,path)?"Loadout applied.":"Error applying loadout.");
  m.description("Replaces your weapons with this loadout.");
  if(m.add("Overwrite File",ActionRow,"")&&m.event()==Accept)tw::notify(createLoadout(ped,path)?"File overwritten.":"Error: Unable to overwrite file.");
  if(m.add("Delete File",ActionRow,"")&&m.event()==Accept){tw::notify(DeleteFileA(path.c_str())?"File deleted.":"Error deleting file.");leave=true;}
  return true;
 }
 bool laserPage(Transport&m){
  m.setTitle("Laser Sight");toggleRow(m,"Toggle",on_[Laser],NULL);
  char rgba[40];sprintf_s(rgba,"%d, %d, %d, %d",laserR_,laserG_,laserB_,laserA_);m.add("Set Colour",ActionRow,rgba,ids_.laserColour);return true;
 }
 bool laserColourPage(Transport&m){
  m.setTitle("Set Colour");int*c[4]={&laserR_,&laserG_,&laserB_,&laserA_};const char*n[4]={"Red","Green","Blue","Opacity"};char t[16];
  for(int i=0;i<4;++i){sprintf_s(t,"%d",*c[i]);if(m.add(n[i],NumberRow,t)&&(m.event()==Left||m.event()==Right))*c[i]=(*c[i]+(m.event()==Right?5:251))%256;}
  return true;
 }
 // Routine weapon effects, once per UPDATE.
 bool tick(){
  const int ped=tw::ped();if(!tw::exists(ped))return true;const int now=tw::timer();const uint32_t weapon=current(ped);
  if(was_[BulletTime]&&!on_[BulletTime]&&worldSpeed_)sn::SET_TIME_SCALE(*worldSpeed_);
  memcpy(was_,on_,sizeof(on_));
  if(on_[InfiniteParachutes])giveParachute(ped);
  if(damage_!=1.f)applyDamage();
  if(on_[RapidFire])rapidTick(ped,weapon);
  if(on_[Triggerbot])triggerTick(ped,weapon);
  if(on_[SoulSwitch]||soulAt_)soulSwitchTick(ped,weapon,now);
  if(on_[RemovalGun])removalTick(ped,weapon);
  if(on_[RevivalGun])revivalTick(ped,weapon);
  if(sn::IS_PED_SHOOTING(ped)){
   if(on_[TeleportGun])teleportTick(ped,weapon);
   if(on_[BulletTime]){sn::SET_TIME_SCALE(.2f);bulletTimeUntil_=now+810;}
   if(on_[Tenfold])tenfoldTick(ped,weapon);
  }
  if(on_[BulletTime]&&bulletTimeUntil_&&now>=bulletTimeUntil_&&worldSpeed_){sn::SET_TIME_SCALE(*worldSpeed_);bulletTimeUntil_=0;}
  if(on_[Laser])laserTick(ped,weapon);
  return true;
 }
 void suspend(){if(on_[BulletTime]&&worldSpeed_)sn::SET_TIME_SCALE(*worldSpeed_);memset(on_,0,sizeof(on_));memset(was_,0,sizeof(was_));soulAt_=0;}
};
}
