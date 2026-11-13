#pragma once
#include "teleport_actions.h"
#include "menu_transport.h"
#include "teleport_locations.inl"
#include "trainer_world.h"
#include "teleport_blip_data.h"
namespace menyoo_vr {
// Reuse original locations and grouping; never dereference upstream SUB page
// sentinels as vector pointers. Interior configurators remain explicit work.
class TeleportPages {
 TeleportActions*actions_;const sub::TeleportLocations_catind::NamedTeleLocationList*category_;
 TeleportState custom_;bool customReady_;
 // Waypoint / Mission Objective / Map Blips (curtis TeleMethods.cpp, Teleport_Submenus.cpp Sub_BlipList). Blips are
 // enumerated with the ORIGINAL GET_FIRST/NEXT_BLIP_INFO_ID natives instead of Menyoo's retail GTAmemory blip pool.
 struct Blip {int sprite,colour;tw::V3 at;};std::vector<Blip>blips_;int blipsAt_;
 int waypointStep_,waypointAt_;float waypointX_,waypointY_,waypointZ_;
 static const char*blipName(int sprite){size_t lo=0,hi=sizeof(blips::NAMES)/sizeof(blips::NAMES[0]);
  while(lo<hi){const size_t mid=(lo+hi)/2;if(blips::NAMES[mid].sprite<sprite)lo=mid+1;else hi=mid;}
  return lo<sizeof(blips::NAMES)/sizeof(blips::NAMES[0])&&blips::NAMES[lo].sprite==sprite?blips::NAMES[lo].name:"Unknown";}
 static float height(int i){static const float h[18]={100,150,50,0,200,250,300,350,400,450,500,550,600,650,700,750,800,850};return h[i];}
 void scan(){
  const int now=tw::timer();if(blipsAt_&&now-blipsAt_<1000)return;blipsAt_=now?now:1;blips_.clear();
  for(int sprite=0;sprite<=521&&blips_.size()<1000;++sprite){
   for(int b=sn::GET_FIRST_BLIP_INFO_ID(sprite);sn::DOES_BLIP_EXIST(b)&&blips_.size()<1000;b=sn::GET_NEXT_BLIP_INFO_ID(sprite)){
    Blip x;x.sprite=sn::GET_BLIP_SPRITE(b);x.colour=sn::GET_BLIP_COLOUR(b);x.at=sn::GET_BLIP_INFO_ID_COORD(b);blips_.push_back(x);}}
 }
 static std::string zone(const tw::V3&p){const char*z=sn::GET_NAME_OF_ZONE(p);return z&&*z?tw::text(z,z):std::string("");}
 bool missionObjective(){
  blipsAt_=0;scan();
  for(size_t i=0;i<blips_.size();++i){const Blip&b=blips_[i];const int c=b.colour,s=b.sprite;
   if(s==blips::IconCrateDrop||(c==blips::ColourYellow&&s==blips::IconStandard)||(c==blips::ColourYellow3&&s==blips::IconStandard)||
      (c==blips::ColourYellow2&&s==blips::IconStandard)||(c==blips::ColourWhite&&s==blips::IconRaceFinish)||
      (c==blips::ColourGreen&&s==blips::IconStandard)||(c==blips::ColourBlue&&s==blips::IconStandard))
    return actions_->coordinates(b.at.x,b.at.y,b.at.z);}
  tw::notify("No mission objective blip found.");return true;
 }
 bool waypoint(){
  if(!sn::IS_WAYPOINT_ACTIVE()){tw::notify("~r~Error:~s~ No Waypoint set.");return true;}
  const tw::V3 at=sn::GET_BLIP_INFO_ID_COORD(sn::GET_FIRST_BLIP_INFO_ID(blips::IconWaypoint));
  waypointX_=at.x;waypointY_=at.y;waypointZ_=at.z;float z=0;if(sn::GET_GROUND_Z_FOR_3D_COORD(tw::v3(at.x,at.y,810.f),&z,false,false))waypointZ_=z;
  waypointStep_=1;waypointAt_=tw::timer();return actions_->coordinates(waypointX_,waypointY_,height(0));
 }
 static void unavailable(Transport&m){m.rows.back().enabled=false;m.description("Native interior configuration pending; no teleport was performed.");}
 bool locations(Transport&m,const std::vector<sub::TeleportLocations_catind::TeleLocation>&list){bool ok=true;
  for(const auto&loc:list){const bool chosen=m.add(loc.name,ActionRow,"");
   if(!loc.iplsToLoad.empty()||!loc.iplsToRemove.empty()||!loc.interiorProps.empty()||loc.bInterior||loc.bOnTheLine||loc.bOffTheLine){unavailable(m);continue;}
   if(chosen&&m.event()==Accept)ok=actions_->coordinates(loc.x,loc.y,loc.z)&&ok;
  }return ok;
 }
public:
 TeleportPages():actions_(NULL),category_(NULL),customReady_(false),blipsAt_(0),waypointStep_(0),waypointAt_(0),waypointX_(0),waypointY_(0),waypointZ_(0){memset(&custom_,0,sizeof(custom_));}
 void bind(TeleportActions&actions){actions_=&actions;category_=NULL;customReady_=false;blips_.clear();blipsAt_=0;waypointStep_=0;}
 bool root(Transport&m,int categoryPage,int customPage,int blipPage=-1){m.setTitle("Teleport Options");bool ok=true;
  if(m.add("Forward",ActionRow,"")&&m.event()==Accept)ok=actions_->forward();
  m.description("Moves your native character or occupied vehicle forward; keeps HMD tracking authoritative.");
  if(blipPage>=0){
   if(m.add("Waypoint",ActionRow,"")&&m.event()==Accept)ok=waypoint()&&ok;
   if(m.add("Mission Objective",ActionRow,"")&&m.event()==Accept)ok=missionObjective()&&ok;
   m.description("Teleports to the current mission objective blip.");
   if(m.add("Map Blips",ActionRow,"",blipPage)&&m.event()==Accept)blipsAt_=0;}
  m.add("---Ready To Go---",BreakRow,"");
  for(const auto&category:sub::TeleportLocations_catind::Locations::vAllCategories)
   if(m.add(category.categoryName,ActionRow,"",categoryPage)&&m.event()==Accept)category_=&category;
  if(m.add("Custom Coordinates",ActionRow,"",customPage)&&m.event()==Accept)customReady_=false;
  return ok;
 }
 bool category(Transport&m){if(!category_){m.setTitle("Locations");m.add("Unavailable",ActionRow,"");return true;}
  m.setTitle(category_->categoryName);bool ok=true;
  if(category_->locList_ptr)ok=locations(m,*category_->locList_ptr);
  const uintptr_t route=reinterpret_cast<uintptr_t>(category_->nextNamedLocListList);
  if(route>=0x4000&&route<0x4100){m.add("Native interior configuration pending",ActionRow,"");unavailable(m);}
  else if(category_->nextNamedLocListList)for(const auto&next:*category_->nextNamedLocListList){m.add(next.categoryName,BreakRow,"");if(next.locList_ptr)ok=locations(m,*next.locList_ptr)&&ok;}
  return ok;
 }
 bool blipList(Transport&m){m.setTitle("Map Blips");scan();bool ok=true;
  for(size_t i=0;i<blips_.size();++i){const Blip&b=blips_[i];if(b.sprite>521)continue;
   if(m.add(std::string(blipName(b.sprite))+" ("+zone(b.at)+")",ActionRow,"")&&m.event()==Accept)ok=actions_->coordinates(b.at.x,b.at.y,b.at.z)&&ok;}
  if(blips_.empty()){m.add("No blips",ActionRow,"");m.rows.back().enabled=false;}
  return ok;
 }
 // Menyoo ToWaypoint height ladder: SET_ENTITY_COORDS at each height, WAIT(100), probe ground; one step per UPDATE.
 bool tick(){
  if(!waypointStep_)return true;const int now=tw::timer();if(now-waypointAt_<100)return true;
  float z=0;const int i=waypointStep_-1;
  if(sn::GET_GROUND_Z_FOR_3D_COORD(tw::v3(waypointX_,waypointY_,height(i)),&z,false,false)){waypointStep_=0;return actions_->coordinates(waypointX_,waypointY_,z);}
  if(waypointStep_>=18){waypointStep_=0;return actions_->coordinates(waypointX_,waypointY_,waypointZ_);}
  waypointAt_=now;++waypointStep_;return actions_->coordinates(waypointX_,waypointY_,height(waypointStep_-1));
 }
 void suspend(){waypointStep_=0;}
 bool custom(Transport&m){m.setTitle("Custom Coordinates");
  if(!customReady_){if(!actions_->current(custom_)){m.add("Unavailable",ActionRow,"");return true;}customReady_=true;}
  if(m.add("Update to Current",ActionRow,"")&&m.event()==Accept)return actions_->current(custom_);
  float*values[]={&custom_.x,&custom_.y,&custom_.z};const char*names[]={"X","Y","Z"};
  for(unsigned i=0;i<3;++i){char text[48];sprintf_s(text,"%.4f",*values[i]);
   if(m.add(names[i],NumberRow,text)&&(m.event()==Right||m.event()==Left))*values[i]+=m.event()==Right?.1f:-.1f;
  }
  return !(m.add("Apply",ActionRow,"")&&m.event()==Accept)||actions_->coordinates(custom_.x,custom_.y,custom_.z);
 }
};
}
