#pragma once
#include "scope_policy.h"
#include <string.h>
// Apply these BEFORE translation and BEFORE the original callback/action.
// Symbols come from the pinned upstream SUB enum, not from translated captions.
namespace menyoo_vr {
inline bool routeIs(const char*route,const char*expected){return route&&strcmp(route,expected)==0;}
inline bool routeStarts(const char*route,const char*prefix){return route&&strncmp(route,prefix,strlen(prefix))==0;}
inline Origin rootOrigin(const char*route,Origin fallback){
 if(routeIs(route,"PLAYEROPS"))return Player;
 if(routeIs(route,"VEHICLEOPS"))return Vehicle;
 if(routeIs(route,"TELEPORTOPS"))return Teleport;
 if(routeIs(route,"WEAPONOPS"))return Weapons;
 if(routeIs(route,"WEATHEROPS"))return Weather;
 if(routeIs(route,"TIMEOPS"))return Time;
 if(routeIs(route,"BODYGUARDMAINMENU")||routeIs(route,"BODYGUARDOPS"))return Bodyguards;
 if(routeIs(route,"SETTINGS"))return Settings;
 return fallback;
}
inline Section pageSection(Origin origin,const char*route,Section fallback=General){
 if(routeStarts(route,"BODYGUARD"))return BodyguardsTab;
 if(routeIs(route,"WATERHACK"))return WaterHackTab;
 if(routeStarts(route,"PLAYERSSUB"))return PlayersTab;
 if(routeIs(route,"SPOONER_MAIN"))return SpoonerTab;
 if(routeIs(route,"MISCOPS"))return MiscTab;
 if(routeIs(route,"CREDITSSUB"))return CreditsTab;
 if(origin==Settings&&routeIs(route,"SETTINGS"))return SettingsRoot;
 if(origin==Settings&&routeStarts(route,"SETTINGS_THEMES"))return ThemeEditor;
 if(origin==Settings&&routeStarts(route,"SETTINGS_COLOURS"))return ColourEditor;
 if(origin!=Player)return fallback; // Shared selectors retain their caller's scope.
 if(routeIs(route,"SPSTATMANAGER"))return BasicStatsCharacters;
 if(routeStarts(route,"SPSTATMANAGER"))return BasicStats;
 if(routeStarts(route,"MODELCHANGER"))return ModelChanger;
 if(routeStarts(route,"COMPONENTS")||routeStarts(route,"PED_HEADFEATURES"))return Wardrobe;
 if(routeStarts(route,"ANIMATIONSUB"))return PlayerAnimations;
 if(routeStarts(route,"AnimationTaskScenarios"))return PlayerScenarios;
 if(routeIs(route,"FACIALMOOD"))return PlayerMoods;
 if(routeIs(route,"MOVEMENTGROUP"))return PlayerMovementStyles;
 if(routeStarts(route,"SPEECHPLAYER"))return PlayerSpeech;
 if(routeIs(route,"VOICECHANGER"))return PlayerVoice;
 if(routeStarts(route,"PEDFLAGMANAGER"))return PlayerPedFlags;
 if(routeStarts(route,"PTFX"))return PlayerFx;
 if(routeIs(route,"BREATHESTUFF"))return PlayerBreathe;
 if(routeIs(route,"GHOSTRIDERMODE"))return PlayerGhostRider;
 return fallback;
}
inline Item targetItem(const Item&context,const char*route){
 Item item=context;
 // Only root transitions change origin. Shared selectors inherit their caller.
 if(context.origin==Root)item.origin=rootOrigin(route,Root);
 const Section section=pageSection(item.origin,route);
 if(section==PlayersTab||section==SpoonerTab||section==MiscTab||section==CreditsTab||section==BodyguardsTab||section==WaterHackTab||
    (item.origin==Player&&section>=PlayerAnimations&&section<=PlayerGhostRider))item.section=section;
 if(item.origin==Player){
  if(routeStarts(route,"PED_HEADFEATURES")){item.section=Wardrobe;item.feature=HeadFeatures;}
  if(routeStarts(route,"COMPONENTS_OUTFITS_DEFAULT")){item.section=Wardrobe;item.feature=DefaultOutfitBeta;}
  else if(routeStarts(route,"COMPONENTS_OUTFITS")){item.section=Wardrobe;item.feature=SaveOutfit;}
 }
 if(routeStarts(route,"MODELCHANGER_FAVOURITES")||routeStarts(route,"SPAWNVEHICLE_FAVOURITES")||
    routeStarts(route,"VEHICLE_SAVER")||routeStarts(route,"COMPONENTS_OUTFITS")||
    routeStarts(route,"WEAPONOPS_LOADOUTS")||routeIs(route,"TELEPORTOPS_SAVEDLOCATIONS"))item.flags|=SavedOrRemembered;
 if(routeIs(route,"MODELCHANGER_SEARCH")||routeIs(route,"SPAWNVEHICLE_SEARCH"))item.flags|=KeyboardInput;
 if(routeStarts(route,"VEHICLESPEEDOS"))item.feature=Speedometers;
 if(routeStarts(route,"PVOPS"))item.feature=PersonalVehicleOptions;
 if(item.origin==Weapons){
  if(routeIs(route,"FORGEGUN"))item.feature=ForgeGun;
  if(routeIs(route,"GRAVITYGUN"))item.feature=GravityGun;
  if(routeIs(route,"KABOOMGUN"))item.feature=KaboomGun;
  if(routeIs(route,"TRIGGERFXGUN"))item.feature=TriggerFxGun;
  if(routeIs(route,"BULLETGUN"))item.feature=BulletGun;
  if(routeStarts(route,"PEDGUN"))item.feature=PedGun;
  if(routeIs(route,"OBJECTGUN"))item.feature=ObjectAndVehicleGun;
 }
 return item;
}
inline Decision routeDecision(Origin origin,const char*route){
 if(origin==Settings&&!routeIs(route,"SETTINGS")&&!routeIs(route,"SETTINGS_LANGUAGE")&&
    !routeStarts(route,"SETTINGS_THEMES")&&!routeStarts(route,"SETTINGS_COLOURS"))return ExcludeSection;
 Item context={origin,pageSection(origin,route),Ordinary,0};
 // Basic Stats requires row filtering, not rejection of the entire editor.
 if(context.section==BasicStats||context.section==BasicStatsCharacters)context.feature=TotalCash;
 if(context.section==SettingsRoot)context.feature=Language;
 return decide(targetItem(context,route));
}
}
