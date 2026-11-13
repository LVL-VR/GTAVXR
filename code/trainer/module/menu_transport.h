#pragma once
#include "scope_policy.h"
#include "submenu_policy.h"
#include "visible_names.h"
#include "localization.h"
#include <string>
#include <vector>
#include <stdio.h>
// Adapter for Menyoo's page callbacks. A frame rebuilds rows using the SAME
// source actions; only input events and visual data are transported to VR.
namespace menyoo_vr {
enum Kind { ActionRow,ToggleRow,NumberRow,ChoiceRow,BreakRow };
enum Event { NoEvent,Accept,Left,Right };
enum Icon { NoIcon,StatusDot,TickIcon,CrossIcon,BoxTickIcon,BoxCrossIcon,BoxBlankIcon,
 CarIcon,BikeIcon,WeaponIcon,TattooIcon,MakeupIcon,MaskIcon,NewStarIcon };
struct Row {std::string label,value,help;Kind kind;int submenu;bool enabled,iconCondition;
 Icon iconTrue,iconFalse;float rotationTrue,rotationFalse;
 // Vehicle Spawner preview sprite for the selected row (empty dictionary = "No preview available").
 bool preview;std::string previewDict,previewTexture;
 Row():kind(ActionRow),submenu(-1),enabled(true),iconCondition(false),iconTrue(NoIcon),iconFalse(NoIcon),rotationTrue(0),rotationFalse(0),preview(false){}
};
inline Feature namedFeature(const std::string&name){
 if(name=="Total Cash")return TotalCash;
 if(name=="Max All Stats (SP)")return MaxAllStats;
 if(name=="Water Hack"||name=="Water Hack (For Waves At Beaches)")return WaterHack;
 if(name=="Input Model")return InputModel;
 if(name=="Front View")return FrontView;
 if(name=="Head Features")return HeadFeatures;
 if(name=="Default Outfit (beta)"||name=="Default Outfits (Beta)")return DefaultOutfitBeta;
 if(name=="Outfits"||name=="Save Outfit")return SaveOutfit;
 if(name=="Load Outfit")return LoadOutfit;
 if(name=="Fly Manual")return FlyManual;if(name=="Burn Mode")return BurnMode;
 if(name=="Collision")return EntityCollision;if(name=="Opacity (Local)")return EntityOpacity;
 if(name.find("Height (Elongation)")==0)return HeightElongation;
 if(name=="Movement Speed (Alt)")return MovementSpeed;if(name=="Noise Level")return NoiseLevel;if(name=="Sweat Level")return SweatLevel;
 if(name=="Speedometers")return Speedometers;if(name=="PV Options")return PersonalVehicleOptions;
 if(name=="Tow Mode"||name=="Tow Mode (ALPHA)")return TowMode;if(name=="Vehicle Jump")return VehicleJump;if(name=="Hydraulics")return Hydraulics;
 if(name=="Give Digiscanner")return Digiscanner;if(name=="Give Briefcase"||name=="Briefcase")return Briefcase;
 if(name=="Rope Gun (Glitchy)"||name=="Rope Gun")return RopeGun;if(name=="Magnet Gun")return MagnetGun;
 if(name=="Flamethrower Fire Extinguisher")return ExtinguisherFlamethrower;
 if(name=="Forge Gun")return ForgeGun;if(name=="Gravity Gun")return GravityGun;if(name=="Light Gun")return LightGun;
 if(name=="TriggerFX Gun")return TriggerFxGun;if(name=="Kaboom Gun")return KaboomGun;if(name=="Bullet Gun")return BulletGun;
 if(name=="Ped Gun")return PedGun;if(name=="Object Gun")return ObjectGun;if(name=="Vehicle Gun")return VehicleGun;
 if(name=="Object & Vehicle Gun")return ObjectAndVehicleGun;
 if(name=="Clock (System Time)")return SystemClock;if(name=="Sync With System Time"||name=="Sync with System Time")return SyncSystemClock;
 if(name=="Language")return Language;if(name=="Theme"||name=="Themes")return Theme;if(name=="Colour Selector"||name=="Menu Colours")return ColourSelector;
 if(name=="Reset Toggles (Most Of Them)"||name=="Reset Toggles")return ResetToggles;if(name=="Reset All Cheats")return ResetAllCheats;
 return Ordinary;
}
inline std::string plainName(const std::string&label){
 std::string out;bool tag=false;for(size_t i=0;i<label.size();++i){if(label[i]=='~'){tag=!tag;continue;}if(!tag)out+=label[i];}return out;
}
class Transport {
 Item context_;unsigned selected_;Event event_;bool previousAllowed_;int destination_;
 const char*(*routeSymbol_)(int);
 uint32_t language_;
public:
 std::string title;std::vector<Row>rows;
 Transport():selected_(0),event_(NoEvent),previousAllowed_(false),destination_(-1),routeSymbol_(NULL),language_(English){context_.origin=Root;context_.section=General;context_.feature=Ordinary;context_.flags=0;}
 bool language(uint32_t value){if(value>Spanish)return false;language_=value;return true;}
 void routeResolver(const char*(*resolver)(int)){routeSymbol_=resolver;}
 void begin(Origin origin,Section section,unsigned selected,Event event){context_.origin=origin;context_.section=section;context_.feature=Ordinary;context_.flags=0;selected_=selected;event_=event;previousAllowed_=false;destination_=-1;title.clear();rows.clear();}
 bool add(const std::string&label,Kind kind,const std::string&value,int submenu=-1,unsigned flags=0,Feature explicitFeature=Ordinary){
  Item item=context_;const std::string name=plainName(label);item.feature=explicitFeature==Ordinary?namedFeature(name):explicitFeature;item.flags=flags;
  if(name.find("ALPHA")!=std::string::npos)item.flags|=Alpha;
  if(name=="Favourites"||name=="Favorites")item.flags|=SavedOrRemembered;
  if(item.feature==InputModel)item.flags|=KeyboardInput;
  if(routeSymbol_&&submenu>=0)item=targetItem(item,routeSymbol_(submenu));
  previousAllowed_=decide(item)==Keep;
  if(!previousAllowed_)return false;
  Row row;row.label=localized(visibleName(name),language_);row.kind=kind;row.value=localized(visibleName(value),language_);row.submenu=submenu;rows.push_back(row);
  const bool selected=rows.size()-1==selected_;
  if(selected&&event_==Accept&&submenu>=0)destination_=submenu;
  return selected;
 }
 void description(const std::string&help){if(previousAllowed_&&!rows.empty())rows.back().help=localized(visibleName(help),language_);}
 void icons(bool condition,Icon on,Icon off,float rotationOn=0,float rotationOff=0){
  if(previousAllowed_&&!rows.empty()){Row&r=rows.back();r.iconCondition=condition;r.iconTrue=on;r.iconFalse=off;r.rotationTrue=rotationOn;r.rotationFalse=rotationOff;}
 }
 void preview(const char*dict,const char*texture){
  if(previousAllowed_&&!rows.empty()){Row&r=rows.back();r.preview=true;r.previewDict=dict?dict:"";r.previewTexture=r.previewDict.empty()?localized("No preview available",language_):std::string(texture?texture:"");}
 }
 void setTitle(const std::string&label){title=localized(visibleName(plainName(label)),language_);}
 Event event()const{return event_;}int destination()const{return destination_;}
};
}
