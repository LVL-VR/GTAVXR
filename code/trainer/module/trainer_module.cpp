// Development module: connects the new ABI/navigation/presentation with reused
// original Time logic, retained Story stats and shared appearance pages.
// Other retained pages are still being ported; this is NOT a complete trainer.
#include "module_runtime.h"
#include "page_registry.h"
#include "presentation_encode.h"
#include "settings_pages.h"
#include "appearance_ini.h"
#include "theme_ini.h"
#include "story_cash.h"
#include "source_stats.h"
#include "max_story_stats.h"
#include "source_host_properties.h"
#include "weather_actions.h"
#include "character_actions.h"
#include "wardrobe_pages.h"
#include "model_favourites.h"
#include "player_actions.h"
#include "clone_actions.h"
#include "replenish_actions.h"
#include "player_routine_actions.h"
#include "vehicle_actions.h"
#include "legacy_cheat_actions.h"
#include "retained_actions.h"
#include "teleport_pages.h"
#include "player_ability_actions.h"
#include "ped_overlay_pages.h"
#include "shared_menu_abi.h"
#include "vehicle_options.h"
#include "vehicle_customs.h"
#include "vehicle_spawner.h"
#include "weapon_options.h"
#include <stdexcept>
#include <string.h>
using namespace menyoo_vr;
static ModuleRuntime runtime;
static Host host={};static Transport menu;static PageRegistry pages;static PageStack stack;
static Appearance appearance=defaultAppearance();static ThemeStyle style=defaultStyle();
static SourceStats stats;static WeatherActions weather;static CharacterActions characters;static Input2 currentInput={};static Frame2 produced={};
static PedActions pedActions;static WardrobePages wardrobe;static PedOverlayPages overlays;
static ModelFavourites favourites;static std::vector<uint32_t>favouriteRows;
static PlayerActions playerActions;
static CloneActions clones;
static ReplenishActions replenishment;
static PlayerRoutineActions playerRoutine;
static PlayerAbilityActions abilities;
static VehicleActions vehicles;
static LegacyCheatActions legacyCheats;
static RetainedActions retained;
static TeleportActions teleportActions;static TeleportPages teleportPages;
// retained Vehicle Options / Menyoo Customs / Vehicle Spawner / Weapon Options (typed host binding).
static VehicleOptions vehicleOptions;static VehicleCustoms vehicleCustoms;static VehicleSpawner vehicleSpawner;static WeaponOptions weaponOptions;
static bool pendingBack=false;static VehiclePageIds vehicleIds={};static WeaponPageIds weaponIds={};
static bool freezeWanted=false;static unsigned frozenWanted=0;
static char settingsPath[512]={};static uint64_t session=0;static DWORD owner=0;
static char actionMessage[512]={};static uint64_t actionMessageUntil=0;
static bool attached=false,executing=false,registered=false,configured=false,dirty=false;
static bool null=false,pauseClock=false,syncClock=false;static int pauseClockH=0,pauseClockM=0;
static float currentTimescale=1.f;static unsigned cashSteps[3]={},colourRole=0;
enum Page { Main=0,PlayerPage=1,CashCharacters=2,CashMichael=3,CashFranklin=4,CashTrevor=5,
 TimePage=6,ClockExcluded=7,SettingsPage=8,ThemesPage=9,ColoursPage=10,ColourPage=11,
 WeatherPage=12,CloudsPage=13,ModelPage=14,ModelItems=15,ProtagonistsPage=16,WardrobePage=17,AccessoriesPage=18,VariationPage=19,FavouritesPage=20,CloningPage=21,
 VehiclePage=22,VehicleSpawnerPage=23,VehicleItemsPage=24,WeaponsPage=25,
 TeleportPage=26,TeleportCategoryPage=27,TeleportCustomPage=28,
 DecalTypesPage=29,DecalZonesPage=30,DecalItemsPage=31,DamagePage=32,DamagePacksPage=33,
 VehWeaponsPage=34,VehMultipliersPage=35,VehNeonsPage=36,VehNeonColourPage=37,VehAutoDrivePage=38,VehSlamPage=39,
 CustomsPage=40,CustomsBennys=41,CustomsCatall=42,CustomsPaints=43,CustomsPaints2=44,CustomsChrome=45,CustomsClassic=46,CustomsMatte=47,
 CustomsMetallic=48,CustomsMetal=49,CustomsChameleon=50,CustomsPearl=51,CustomsUtil=52,CustomsWorn=53,CustomsShared=54,CustomsRgb=55,
 CustomsWheels=56,CustomsWheels2=57,CustomsWheels3=58,CustomsNeons=59,CustomsDoors=60,CustomsWindows=61,CustomsEmblem=62,CustomsRemovable=63,
 CustomsLights=64,CustomsEngineSound=65,CustomsEngineSoundItems=66,
 SpawnSettingsPage=67,FunnyPage=68,SpawnDlcPage=69,SpawnDlcItemsPage=70,
 WeaponIndivsPage=71,WeaponCategoryPage=72,WeaponItemPage=73,WeaponModsPage=74,WeaponParachutePage=75,WeaponSmokePage=76,
 WeaponFavouritesPage=77,WeaponLoadoutsPage=78,WeaponLoadoutPage=79,WeaponLaserPage=80,WeaponLaserColourPage=81,TeleportBlipsPage=82 };
static unsigned modelGroup=0;
namespace SUB {enum {CLOCK=ClockExcluded};}
static void nullFunc(){}
static void copy(char*output,size_t capacity,const std::string&text){
 if(!capacity)return;size_t count=text.size()<capacity-1?text.size():capacity-1;
 while(count&&count<text.size()&&(static_cast<unsigned char>(text[count])&0xc0)==0x80)--count;
 memcpy(output,text.data(),count);output[count]=0;
}
static bool sourceCaller(void*,uint64_t hash,const uint64_t*arguments,unsigned count,uint64_t*result){
 if(!executing||!attached||!host.invoke)return false;return host.invoke(host.user,hash,arguments,count,result)==Success;
}
static uint64_t rawNative(uint64_t hash,const uint64_t*arguments,unsigned count){
 if(!executing||!attached||!host.invoke)throw std::runtime_error("TRAINER_NATIVE_OUTSIDE_OWNED_TICK");
 uint64_t result[3]={};const int status=host.invoke(host.user,hash,arguments,count,result);
 if(status!=Success)throw std::runtime_error(status==MissingNative?"TRAINER_NATIVE_NOT_ADMITTED":"TRAINER_NATIVE_CONTEXT_REJECTED");
 return result[0];
}
static ResetAccess readTime(void*,const OverrideOwner&ownerValue,uint64_t&value){
 if(ownerValue.session!=session)return ResetOwnerGone;
 if(!executing||!host.invoke)return ResetUnavailable;
 uint64_t result[3]={};const int status=host.invoke(host.user,SourceHostReadScriptTimeScale,NULL,0,result);
 if(status!=Success)return ResetUnavailable;
 float decoded=0;memcpy(&decoded,result,sizeof(decoded));if(!(decoded>=0&&decoded<=1.1f))return ResetUnavailable;
 value=result[0]&UINT64_C(0xffffffff);return ResetOk;
}
static ResetAccess writeTime(void*,const OverrideOwner&ownerValue,uint64_t value){
 if(ownerValue.session!=session)return ResetOwnerGone;
 if(!executing||!host.invoke)return ResetUnavailable;
 float decoded=0;memcpy(&decoded,&value,sizeof(decoded));if(!(decoded>=0&&decoded<=1.1f))return ResetUnavailable;
 uint64_t args[1]={value},result[3]={};
 return host.invoke(host.user,UINT64_C(0x434e3ae126ddc5b5),args,1,result)==Success?ResetOk:ResetUnavailable;
}
static uint64_t native(uint64_t hash,const uint64_t*arguments,unsigned count){
 if(hash==UINT64_C(0x434e3ae126ddc5b5)){
  if(count!=1||!arguments)throw std::runtime_error("TRAINER_TIME_ARGUMENTS");
  const OverrideOwner owned={session,1,session};
  if(runtime.resets().apply(1,owned,ExplicitCheatOverride,NULL,readTime,writeTime,arguments[0])!=ResetOk)
   throw std::runtime_error("TRAINER_TIME_OVERRIDE_NOT_OWNED");
  return 0;
 }
 return rawNative(hash,arguments,count);
}
#include "time_native_bindings.inl"
static void AddTitle(const char*label){menu.setTitle(label);}
static void AddOptionDescription(const char*help){menu.description(help);}
static void AddBreak(const char*label){menu.add(label,BreakRow,"");}
static void AddOption(const char*label,bool&pressed,void(&callback)()=nullFunc,int submenu=-1){
 if(menu.add(label,ActionRow,"",submenu)&&menu.event()==Accept){pressed=true;callback();}
}
static void AddToggle(const char*label,bool&value,bool&pressed=null){
 if(menu.add(label,ToggleRow,value?"ON":"OFF")&&menu.event()==Accept){value=!value;pressed=true;}
}
static void AddNumber(const char*label,double value,int decimals,bool&accept,bool&right,bool&left){
 char formatted[64];sprintf_s(formatted,"%.*f",decimals,value);
 if(menu.add(label,NumberRow,formatted)){accept=menu.event()==Accept;right=menu.event()==Right;left=menu.event()==Left;}
}
#include "upstream-page.inl"
static const char*routeSymbol(int id){const PageRegistration*r=pages.find(id);return r?r->symbol:NULL;}
static void showActionMessage(const char*text);
static bool mainPage(void*,Transport&m){
 // retained Menyoo source order (Player, Vehicle, Teleport, Weapon, Weather, Time, Settings).
 m.setTitle("Trainer");m.add("Player Options",ActionRow,"",PlayerPage);m.add("Vehicle Options",ActionRow,"",VehiclePage);
 m.add("Teleport Options",ActionRow,"",TeleportPage);m.add("Weapon Options",ActionRow,"",WeaponsPage);m.add("Weather Options",ActionRow,"",WeatherPage);
 m.add("Time Options",ActionRow,"",TimePage);m.add("Settings",ActionRow,"",SettingsPage);
 // build137 closure: "Previous Trainer Options" is retired; every retained function now lives in these pages.
 return true;
}
static bool teleportPage(void*,Transport&m){if(!teleportPages.root(m,TeleportCategoryPage,TeleportCustomPage,TeleportBlipsPage))showActionMessage("Teleport incomplete; native state was preserved where possible.");return true;}
static bool teleportCategoryPage(void*,Transport&m){if(!teleportPages.category(m))showActionMessage("Teleport incomplete; native state was preserved where possible.");return true;}
static bool teleportCustomPage(void*,Transport&m){if(!teleportPages.custom(m))showActionMessage("Teleport incomplete; native state was preserved where possible.");return true;}
static bool teleportBlipsPage(void*,Transport&m){if(!teleportPages.blipList(m))showActionMessage("Teleport incomplete; native state was preserved where possible.");return true;}
// Vehicle Options (VEHICLEOPS and children), Menyoo Customs, Vehicle Spawner and Weapon Options.
static bool vehiclePage(void*,Transport&m){return vehicleOptions.page(m);}
static bool vehWeaponsPage(void*,Transport&m){return vehicleOptions.weaponsPage(m);}
static bool vehMultipliersPage(void*,Transport&m){return vehicleOptions.multipliersPage(m);}
static bool vehNeonsPage(void*,Transport&m){return vehicleOptions.neonsPage(m);}
static bool vehNeonColourPage(void*,Transport&m){return vehicleOptions.neonColourPage(m);}
static bool vehAutoDrivePage(void*,Transport&m){return vehicleOptions.autoDrivePage(m);}
static bool vehSlamPage(void*,Transport&m){return vehicleOptions.slamPage(m);}
// Menyoo s_selectedVehicleHandle: the vehicle you are in when the Customs root is built; paint parts return to vehicle mode.
static bool customsPage(void*,Transport&m){vehicleCustoms.editVehicle();vehicleCustoms.target(tw::vehicle());return vehicleCustoms.page(m);}
static bool customsBennys(void*,Transport&m){return vehicleCustoms.bennysPage(m);}
static bool customsCatall(void*,Transport&m){return vehicleCustoms.catallPage(m);}
static bool customsPaints(void*,Transport&m){return vehicleCustoms.paintsPage(m);}
static bool customsPaints2(void*,Transport&m){return vehicleCustoms.paints2Page(m);}
static bool customsList(void*,Transport&m){static const int listPages[9]={CustomsChrome,CustomsClassic,CustomsMatte,CustomsMetallic,CustomsMetal,CustomsChameleon,CustomsPearl,CustomsUtil,CustomsWorn};
 for(int i=0;i<9;++i)if(stack.current().page==listPages[i])return vehicleCustoms.listPage(m,i);
 return true;}
static bool customsShared(void*,Transport&m){return vehicleCustoms.sharedPage(m);}
static bool customsRgb(void*,Transport&m){return vehicleCustoms.rgbPage(m);}
static bool customsWheels(void*,Transport&m){return vehicleCustoms.wheelsPage(m);}
static bool customsWheels2(void*,Transport&m){return vehicleCustoms.wheels2Page(m);}
static bool customsWheels3(void*,Transport&m){return vehicleCustoms.wheels3Page(m);}
static bool customsNeons(void*,Transport&m){return vehicleCustoms.neonsPage(m);}
static bool customsDoors(void*,Transport&m){return vehicleCustoms.doorsPage(m);}
static bool customsWindows(void*,Transport&m){return vehicleCustoms.windowsPage(m);}
static bool customsEmblem(void*,Transport&m){return vehicleCustoms.emblemPage(m);}
static bool customsRemovable(void*,Transport&m){return vehicleCustoms.removablePage(m);}
static bool customsLights(void*,Transport&m){return vehicleCustoms.lightsPage(m);}
static bool customsEngineSound(void*,Transport&m){return vehicleCustoms.engineSoundPage(m,vehicles);}
static bool customsEngineSoundItems(void*,Transport&m){return vehicleCustoms.engineSoundItemsPage(m,vehicles);}
static bool vehicleSpawnerPage(void*,Transport&m){return vehicleSpawner.page(m);}
static bool vehicleItemsPage(void*,Transport&m){return vehicleSpawner.categoryPage(m);}
static bool spawnSettingsPage(void*,Transport&m){return vehicleSpawner.settingsPage(m);}
static bool funnyPage(void*,Transport&m){return vehicleSpawner.funnyPage(m);}
static bool spawnDlcPage(void*,Transport&m){return vehicleSpawner.dlcPage(m);}
static bool spawnDlcItemsPage(void*,Transport&m){return vehicleSpawner.dlcItemsPage(m);}
// Rows whose accepted implementation already exists (retained EXE actions, legacy cheat owners) keep that implementation.
static void weaponHostRow(void*,Transport&m,WeaponHostRow row){
 switch(row){
 case WeaponGiveAll:if(m.add("Give All Weapons",ActionRow,"")&&m.event()==Accept&&retained.apply(TrainerGiveAllWeapons)!=Success)showActionMessage("Weapon action unavailable in the current native state.");break;
 case WeaponMaxAmmo:if(m.add("Give Max Ammo",ActionRow,"")&&m.event()==Accept&&retained.apply(TrainerGiveMaxAmmo)!=Success)showActionMessage("Weapon action unavailable in the current native state.");break;
 case WeaponInfiniteClip:if(!legacyCheats.row(m,LegacyInfiniteClip,"Infinite Ammo In Clip"))showActionMessage("Weapon action unavailable in the current native state.");m.description("Never need to reload.");break;
 case WeaponInfiniteAmmo:if(!legacyCheats.row(m,LegacyInfiniteAmmo,"Infinite Ammo"))showActionMessage("Weapon action unavailable in the current native state.");break;
 case WeaponExplosiveMelee:if(!legacyCheats.row(m,LegacyExplosiveMelee,"Explosive Melee"))showActionMessage("Weapon action unavailable in the current native state.");m.description("Punches and melee hits cause explosions.");break;
 case WeaponExplosiveAmmo:if(!legacyCheats.row(m,LegacyExplosiveAmmo,"Explosive Ammo"))showActionMessage("Weapon action unavailable in the current native state.");break;
 case WeaponFlaming:if(!legacyCheats.row(m,LegacyFireAmmo,"Flaming Bullets"))showActionMessage("Weapon action unavailable in the current native state.");break;
 }
}
static bool weaponsPage(void*,Transport&m){return weaponOptions.page(m,weaponHostRow,NULL);}
static bool weaponIndivsPage(void*,Transport&m){return weaponOptions.indivsPage(m);}
static bool weaponCategoryPage(void*,Transport&m){return weaponOptions.categoryPage(m);}
static bool weaponItemPage(void*,Transport&m){return weaponOptions.itemPage(m);}
static bool weaponModsPage(void*,Transport&m){return weaponOptions.modsPage(m);}
static bool weaponParachutePage(void*,Transport&m){return weaponOptions.parachutePage(m);}
static bool weaponSmokePage(void*,Transport&m){return weaponOptions.smokePage(m);}
static bool weaponFavouritesPage(void*,Transport&m){return weaponOptions.favouritesPage(m);}
static bool weaponLoadoutsPage(void*,Transport&m){return weaponOptions.loadoutsPage(m);}
static bool weaponLoadoutPage(void*,Transport&m){bool leave=false;const bool ok=weaponOptions.loadoutPage(m,leave);if(leave)pendingBack=true;return ok;}
static bool weaponLaserPage(void*,Transport&m){return weaponOptions.laserPage(m);}
static bool weaponLaserColourPage(void*,Transport&m){return weaponOptions.laserColourPage(m);}
static bool playerPage(void*,Transport&m){
 m.setTitle("Player Options");m.add("Basic Stats",ActionRow,"",CashCharacters);
 if(m.add("Max All Stats (SP)",ActionRow,"")&&m.event()==Accept){
  if(!maxAllStoryStats(stats))throw std::runtime_error("TRAINER_MAX_STATS_INCOMPLETE");
 }
 m.description("Maximizes Story stats for Michael, Franklin and Trevor. Resets do not undo them.");
 m.add("Model Changer",ActionRow,"",ModelPage);m.add("Wardrobe",ActionRow,"",WardrobePage);
 m.add("Cloning Options",ActionRow,"",CloningPage);
 if(m.add("Replenish Player",ActionRow,"")&&m.event()==Accept&&!replenishment.apply())showActionMessage("Replenishment incomplete; applied changes are not automatically undone.");
 m.description("Restores native maximum health and armour and clears visual damage. Resets do not undo this action.");
 // source order (PlayerOptions.cpp): Refill, Invincibility..Seatbelt, Unlimited Special, Auto-Clean,
 // Super Run, Super Jump, Walk underwater, Forcefield, Smash, Fly Auto, Ignored By Everyone, Wanted.
 playerRoutine.row(m,0);
 if(!playerActions.page(m))showActionMessage("Player change rejected; native state was preserved.");
 if(!legacyCheats.row(m,LegacyUnlimitedAbility,"Unlimited Special Ability (SP)"))showActionMessage("Player change rejected; native state was preserved.");
 playerRoutine.row(m,1);abilities.superRun(m);playerRoutine.row(m,2);
 abilities.walkUnderwater(m);abilities.forcefield(m);abilities.smash(m);abilities.flyAuto(m);abilities.ignored(m);
 unsigned wanted=0;const bool known=retained.wanted(wanted);char stars[16];sprintf_s(stars,"%u",wanted);
 if(m.add("Wanted Level",NumberRow,stars)&&known&&(m.event()==Left||m.event()==Right)){
  if(!retained.adjustWanted(m.event()==Right))showActionMessage("Player change rejected; native state was preserved.");
  else if(freezeWanted){if(retained.wanted(frozenWanted)&&!frozenWanted)freezeWanted=false;}
 }
 if(!known)m.rows.back().enabled=false;
 if(wanted==0&&!freezeWanted){if(!legacyCheats.row(m,LegacyNeverWanted,"Never Wanted"))showActionMessage("Player change rejected; native state was preserved.");}
 else {if(m.add("Freeze Wanted Level",ToggleRow,freezeWanted?"ON":"OFF")&&m.event()==Accept){freezeWanted=!freezeWanted;if(freezeWanted)frozenWanted=wanted;}
  m.description("Keeps the selected native wanted level. Reset stops this effect; it does not undo wanted-level edits.");}return true;
}
static bool cashCharacters(void*,Transport&m){
 m.setTitle("Basic Stats");for(unsigned i=0;i<3;++i)m.add(storyName(i),ActionRow,"",CashMichael+static_cast<int>(i),0,StoryCashCharacter);return true;
}
static bool cashPage(void*,Transport&m){const unsigned character=static_cast<unsigned>(stack.current().page-CashMichael);storyCashPage(m,stats,character,cashSteps[character]);return true;}
static bool timePage(void*,Transport&){
 uint64_t value=0;const OverrideOwner own={session,1,session};if(readTime(NULL,own,value)!=ResetOk)throw std::runtime_error("TRAINER_TIME_STATE_UNAVAILABLE");
 memcpy(&currentTimescale,&value,sizeof(currentTimescale));sub::TimeMenu();return true;
}
static void showActionMessage(const char*text){copy(actionMessage,sizeof(actionMessage),localized(text,appearance.language));actionMessageUntil=currentInput.milliseconds+2500;}
static bool weatherPage(void*,Transport&m){if(!weather.page(m,CloudsPage))showActionMessage("Weather change rejected; current state was preserved.");return true;}
static bool cloudsPage(void*,Transport&m){if(!weather.clouds(m))showActionMessage("Cloud change rejected; current state was preserved.");return true;}
static const char*modelGroupName(unsigned group){const char*names[]={"Player","Story Models","Cutscene Models","Multiplayer Models","Animals",
 "Ambient Females","Ambient Males","Gang Female","Gang Males","Scenario Females","Scenario Males","Story Scenario Females","Story Scenario Males","Others"};return group<14?names[group]:"Others";}
static void characterActionStatus(int status){if(status==Success)showActionMessage("Native character request submitted.");else showActionMessage("Character change unavailable in the current native state.");}
static bool modelPage(void*,Transport&m){
 m.setTitle("Model Changer");m.add("Switch Protagonist",ActionRow,"",ProtagonistsPage);
 if(m.add("Restore Story Character",ActionRow,"")&&m.event()==Accept)characterActionStatus(characters.restore());
 if(m.add("Refresh Installed Models",ActionRow,"")&&m.event()==Accept)characters.refresh();
 m.add("Favourites",ActionRow,"",FavouritesPage);
 if(m.add("Randomize Ped Variation",ActionRow,"")&&m.event()==Accept){
  if(!pedActions.utility(0))showActionMessage("Appearance change rejected; native state was preserved where possible.");}
 m.description("Randomizes clothing and features for the current model.");
 if(!characters.load()){m.add("Unavailable",ActionRow,"");m.rows.back().enabled=false;return true;}
 m.add("---Categories---",BreakRow,"");
 for(unsigned group=0;group<14;++group){const unsigned count=characters.count(group);char value[32];sprintf_s(value,"%u",count);
  if(m.add(modelGroupName(group),ActionRow,value,ModelItems)&&m.event()==Accept&&count)modelGroup=group;
  if(!count)m.rows.back().enabled=false;
 }return true;
}
static bool modelItemsPage(void*,Transport&m){
 m.setTitle(modelGroupName(modelGroup));const unsigned count=characters.count(modelGroup);
 if(!count){m.add("Unavailable",ActionRow,"");m.description("Use Back to return.");return true;}
 for(unsigned i=0;i<count;++i){const CatalogueEntry*row=characters.entry(modelGroup,i);
  if(row&&m.add(row->name,ActionRow,"")&&m.event()==Accept)characterActionStatus(characters.request(modelGroup,i));
  if(row){m.icons(favourites.contains(row->hash),NewStarIcon,NoIcon);m.description("Y: add or remove this model from favourites.");}
 }return true;
}
static const CatalogueEntry*pedByHash(uint32_t hash){return characters.byHash(hash);}
static bool favouritesPage(void*,Transport&m){
 m.setTitle("Favourites");favouriteRows.clear();
 const std::vector<ModelFavourites::Entry>&saved=favourites.entries();
 for(unsigned i=0;i<saved.size();++i){const CatalogueEntry*row=pedByHash(saved[i].hash);favouriteRows.push_back(saved[i].hash);
  if(row){if(m.add(saved[i].name.empty()?row->name:saved[i].name,ActionRow,"")&&m.event()==Accept){
    const unsigned group=CharacterActions::group(*row);for(unsigned n=0;n<characters.count(group);++n)if(characters.entry(group,n)==row){characterActionStatus(characters.request(group,n));break;}}
  }else {char label[96];sprintf_s(label,"%s: 0x%08X",localized("Missing model",appearance.language).c_str(),saved[i].hash);m.add(label,ActionRow,"");}
  m.icons(true,NewStarIcon,NoIcon);m.description("Y: add or remove this model from favourites.");}
 if(favouriteRows.empty()){m.add("No installed favourites",ActionRow,"");m.description("Select a model and press Y to add it to favourites.");}return true;
}
static void toggleModelFavourite(){
 const CatalogueEntry*row=NULL;const unsigned selected=stack.current().selected;
 // Menyoo Weapons favourites (menu_action on a weapon row / remove on a favourites row).
 if(stack.current().page==WeaponCategoryPage){weaponOptions.toggleFavourite(selected);return;}
 if(stack.current().page==WeaponFavouritesPage){weaponOptions.removeFavouriteRow(selected,weaponOptions.favouriteRowOffset());return;}
 if(stack.current().page==ModelItems)row=characters.entry(modelGroup,selected);
 else if(stack.current().page==FavouritesPage&&selected<favouriteRows.size())row=pedByHash(favouriteRows[selected]);
 if(!row){if(stack.current().page==FavouritesPage&&selected<favouriteRows.size()){
   if(favourites.toggle(favouriteRows[selected],"missing_model"))showActionMessage("Favourites saved.");else showActionMessage("Could not save favourites; existing data was preserved.");}return;}
 if(favourites.toggle(row->hash,row->name))showActionMessage("Favourites saved.");else showActionMessage("Could not save favourites; existing data was preserved.");
}
static bool protagonistsPage(void*,Transport&m){m.setTitle("Switch Protagonist");for(unsigned i=0;i<3;++i)
 if(m.add(storyName(i),ActionRow,"")&&m.event()==Accept)characterActionStatus(characters.switchStory(i));return true;}
static void wardrobeOverlayRows(Transport&m){overlays.rootRows(m,DecalTypesPage,DamagePage);}
static bool decalTypesPage(void*,Transport&m){if(!overlays.types(m,DecalZonesPage))showActionMessage("Appearance change rejected; native state was preserved where possible.");return true;}
static bool decalZonesPage(void*,Transport&m){overlays.zones(m,DecalItemsPage);return true;}
static bool decalItemsPage(void*,Transport&m){if(!overlays.items(m))showActionMessage("Appearance change rejected; native state was preserved where possible.");return true;}
static bool damagePage(void*,Transport&m){if(!overlays.damage(m,DamagePacksPage))showActionMessage("Appearance change rejected; native state was preserved where possible.");return true;}
static bool damagePacksPage(void*,Transport&m){if(!overlays.damagePacks(m))showActionMessage("Appearance change rejected; native state was preserved where possible.");return true;}
static bool wardrobePage(void*,Transport&m){if(!wardrobe.root(m,AccessoriesPage,VariationPage,currentInput.milliseconds,wardrobeOverlayRows))showActionMessage("Appearance change rejected; native state was preserved where possible.");return true;}
static bool accessoriesPage(void*,Transport&m){if(!wardrobe.accessories(m,VariationPage,currentInput.milliseconds))showActionMessage("Appearance change rejected; native state was preserved where possible.");return true;}
static bool variationPage(void*,Transport&m){if(!wardrobe.editor(m))showActionMessage("Appearance change rejected; native state was preserved where possible.");return true;}
static bool cloningPage(void*,Transport&m){
 m.setTitle("Cloning Options");const char*labels[]={"Clone As Neutral","Clone As Companion (7 Max)","Clone As Enemy"};
 const char*help[]={"Clone ignores you.","Clone follows and fights for you (max 7).","Clone attacks you."};
 for(unsigned i=0;i<3;++i){if(m.add(labels[i],ActionRow,"")&&m.event()==Accept){int created=0;
   if(!clones.clone(i,created))showActionMessage(created?"Clone created, but some setup could not complete.":"Clone unavailable in this native state.");
   else showActionMessage("Native clone created.");}m.description(help[i]);}
 return true;
}
// close stops the Player abilities (host restores its captures) before releasing owned clones.
static bool releaseClonesOnClose(void*){const bool stopped=abilities.release();const bool released=clones.release();
 vehicleSpawner.suspend();weaponOptions.suspend();teleportPages.suspend();const bool vehicle=vehicleOptions.release();return stopped&&released&&vehicle;}
static bool settingsPage(void*,Transport&m){
 const SettingsPageResult result=sharedAppearancePage(m,appearance);dirty=dirty||result.changed;
 if(result.route==SettingsThemes)stack.enter(ThemesPage,pages);
 else if(result.route==SettingsColours)stack.enter(ColoursPage,pages);
 else if(result.route==SettingsResetToggles||result.route==SettingsResetAll){
  const ResetReport report=runtime.resets().reset(result.route==SettingsResetAll);
  if(!report.complete())copy(produced.message,sizeof(produced.message),localized(report.pending?"Reset pending: owned state unavailable.":"Reset preserved values changed by another owner.",appearance.language));
 }
 return true;
}
static bool themesPage(void*,Transport&m){dirty=sharedThemesPage(m,appearance,style)||dirty;return true;}
static bool coloursPage(void*,Transport&m){
 m.setTitle("Colour Selector");for(unsigned i=0;i<ThemeColourCount;++i){
  if(m.add(themeColourName(i),ActionRow,"",ColourPage)&&m.event()==Accept)colourRole=i;
 }return true;
}
static bool colourPage(void*,Transport&m){dirty=sharedColourPage(m,style,colourRole)||dirty;return true;}
static bool registerPages(){
 if(registered)return true;const PageRegistration entries[]={
  {Main,"MAINMENU",mainPage},{PlayerPage,"PLAYEROPS",playerPage},{CashCharacters,"SPSTATMANAGER",cashCharacters},
  {CashMichael,"SPSTATMANAGER_MICHAEL",cashPage},{CashFranklin,"SPSTATMANAGER_FRANKLIN",cashPage},{CashTrevor,"SPSTATMANAGER_TREVOR",cashPage},
  {TimePage,"TIMEOPS",timePage},{SettingsPage,"SETTINGS",settingsPage},{ThemesPage,"SETTINGS_THEMES",themesPage},
  {ColoursPage,"SETTINGS_COLOURS",coloursPage},{ColourPage,"SETTINGS_COLOURS_EDIT",colourPage},
  {WeatherPage,"WEATHEROPS",weatherPage},{CloudsPage,"CLOUDOPS",cloudsPage},
  {ModelPage,"MODELCHANGER",modelPage},{ModelItems,"MODELCHANGER_INSTALLED",modelItemsPage},{ProtagonistsPage,"MODELCHANGER_PLAYER",protagonistsPage},
  {WardrobePage,"COMPONENTS",wardrobePage},{AccessoriesPage,"COMPONENTSPROPS",accessoriesPage},{VariationPage,"COMPONENTS_ITEM",variationPage},
  {FavouritesPage,"MODELCHANGER_FAVOURITES",favouritesPage},{CloningPage,"CLONECOMPANIONSUB",cloningPage},
  {VehiclePage,"VEHICLEOPS",vehiclePage},{VehicleSpawnerPage,"SPAWNVEHICLE",vehicleSpawnerPage},{VehicleItemsPage,"SPAWNVEHICLE_ALLCATS",vehicleItemsPage},{WeaponsPage,"WEAPONOPS",weaponsPage},
  {VehWeaponsPage,"VEHICLEWEAPONS",vehWeaponsPage},{VehMultipliersPage,"VEHICLEMULTIPLIERS",vehMultipliersPage},{VehNeonsPage,"MULTIPLATNEONS",vehNeonsPage},
  {VehNeonColourPage,"MULTIPLATNEONS_COL",vehNeonColourPage},{VehAutoDrivePage,"AUTODRIVESUB",vehAutoDrivePage},{VehSlamPage,"VEHICLESLAM",vehSlamPage},
  {CustomsPage,"MODSHOP",customsPage},{CustomsBennys,"MS_BENNYS",customsBennys},{CustomsCatall,"MSCATALL",customsCatall},{CustomsPaints,"MSPAINTS",customsPaints},
  {CustomsPaints2,"MSPAINTS2",customsPaints2},{CustomsChrome,"MSPAINTS2_CHROME",customsList},{CustomsClassic,"MSPAINTS2_NORMAL",customsList},
  {CustomsMatte,"MSPAINTS2_MATTE",customsList},{CustomsMetallic,"MSPAINTS2_METALLIC",customsList},{CustomsMetal,"MSPAINTS2_METAL",customsList},
  {CustomsChameleon,"MSPAINTS2_CHAMELEON",customsList},{CustomsPearl,"MSPAINTS2_PEARL",customsList},{CustomsUtil,"MSPAINTS2_UTIL",customsList},
  {CustomsWorn,"MSPAINTS2_WORN",customsList},{CustomsShared,"MSPAINTS2_SHARED",customsShared},{CustomsRgb,"MSPAINTS_RGB",customsRgb},
  {CustomsWheels,"MSWHEELS",customsWheels},{CustomsWheels2,"MSWHEELS2",customsWheels2},{CustomsWheels3,"MSWHEELS3",customsWheels3},
  {CustomsNeons,"MSNEONS",customsNeons},{CustomsDoors,"MSDOORS",customsDoors},{CustomsWindows,"MSWINDOWS",customsWindows},{CustomsEmblem,"MS_EMBLEM",customsEmblem},
  {CustomsRemovable,"MSREMOVABLECOMPONENTS",customsRemovable},{CustomsLights,"MSLIGHTS",customsLights},{CustomsEngineSound,"MSENGINESOUND",customsEngineSound},
  {CustomsEngineSoundItems,"MSENGINESOUND_ITEMS",customsEngineSoundItems},
  {SpawnSettingsPage,"SPAWNVEHICLE_OPTIONS",spawnSettingsPage},{FunnyPage,"FUNNYVEHICLES",funnyPage},{SpawnDlcPage,"SPAWNVEHICLE_DLC_SELECTION",spawnDlcPage},
  {SpawnDlcItemsPage,"SPAWNVEHICLEDLC",spawnDlcItemsPage},
  {WeaponIndivsPage,"WEAPONOPS_INDIVS_CATEGORIES",weaponIndivsPage},{WeaponCategoryPage,"WEAPONOPS_INDIVS_CATEGORY",weaponCategoryPage},
  {WeaponItemPage,"WEAPONOPS_INDIVS_ITEM",weaponItemPage},{WeaponModsPage,"WEAPONOPS_INDIVS_ITEM_MODS",weaponModsPage},
  {WeaponParachutePage,"WEAPONOPS_PARACHUTE",weaponParachutePage},{WeaponSmokePage,"WEAPONOPS_PARACHUTE_SMOKE",weaponSmokePage},
  {WeaponFavouritesPage,"WEAPONOPS_WEAPONFAVOURITES",weaponFavouritesPage},{WeaponLoadoutsPage,"WEAPONOPS_LOADOUTS",weaponLoadoutsPage},
  {WeaponLoadoutPage,"WEAPONOPS_LOADOUTS_INITEM",weaponLoadoutPage},{WeaponLaserPage,"WEAPONOPS_LASERSIGHT",weaponLaserPage},
  {WeaponLaserColourPage,"WEAPONOPS_LASERSIGHT_COLOUR",weaponLaserColourPage},{TeleportBlipsPage,"TELEPORTOPS_BLIPLIST",teleportBlipsPage},
  {TeleportPage,"TELEPORTOPS",teleportPage},{TeleportCategoryPage,"TELEPORTOPS_SELECTEDCATEGORY",teleportCategoryPage},{TeleportCustomPage,"TELEPORTOPS_CUSTOMCOORDS",teleportCustomPage},
  {DecalTypesPage,"PEDDECALS_TYPES",decalTypesPage},{DecalZonesPage,"PEDDECALS_ZONES",decalZonesPage},{DecalItemsPage,"PEDDECALS_INZONE",decalItemsPage},
  {DamagePage,"PEDDAMAGET_CATEGORYLIST",damagePage},{DamagePacksPage,"PEDDAMAGET_DAMAGEPACKS",damagePacksPage}};
 for(unsigned i=0;i<sizeof(entries)/sizeof(entries[0]);++i)if(!pages.add(entries[i]))return false;
 registered=true;menu.routeResolver(routeSymbol);return true;
}
static void buildPage(Event event){
 menu.language(appearance.language);const PageLocation location=stack.current();
 if(!pages.dispatch(location.page,location.origin,NULL,menu,location.selected,event))throw std::runtime_error("TRAINER_PAGE_REJECTED");
}
static void renderPage(){
 unsigned selected=stack.current().selected;repairSelection(menu.rows,selected);stack.select(selected);
 if(actionMessage[0]&&currentInput.milliseconds<actionMessageUntil){menu.rows[selected].help=actionMessage;copy(produced.message,sizeof(produced.message),actionMessage);}
 if(!encodePresentation(present(menu,style,selected),currentInput.generation,produced.drawing))throw std::runtime_error("TRAINER_PRESENTATION_INVALID");
 produced.language=appearance.language;produced.page=static_cast<uint32_t>(stack.current().page);
}
static void routine(void*){
 for(;;){
  try{
   // Time toggle runs independently of which page is visible, once per UPDATE.
   if(pauseClock)NETWORK_OVERRIDE_CLOCK_TIME(pauseClockH,pauseClockM,0); // original Routine behavior
   if(freezeWanted&&!retained.setWanted(frozenWanted)){freezeWanted=false;showActionMessage("Player effect paused: native operation unavailable.");}
   if(!playerRoutine.tick(currentInput.generation)){playerRoutine.suspend();showActionMessage("Player effect paused: native operation unavailable.");}
   if(!abilities.tick(currentInput.generation)){abilities.suspend();showActionMessage("Player effect paused: native operation unavailable.");}
   // retained Vehicle/Customs/Spawner/Weapon/Teleport effects and jobs, once per UPDATE.
   if(!vehicleOptions.tick()){vehicleOptions.suspend();showActionMessage("Vehicle effect paused: native operation unavailable.");}
   vehicleCustoms.tick();vehicleSpawner.tick();
   if(!weaponOptions.tick()){weaponOptions.suspend();showActionMessage("Weapon effect paused: native operation unavailable.");}
   if(!teleportPages.tick())showActionMessage("Teleport incomplete; native state was preserved where possible.");
   if(currentInput.visible){
    if(currentInput.event==ModuleOpenSettings||currentInput.event==ModuleOpenTrainer){
     stack.reset(Main);if(currentInput.event==ModuleOpenSettings)stack.enter(SettingsPage,pages);
    }
    if(currentInput.event==ModuleBack){wardrobe.clearConfirmation();if(!stack.back())produced.hideRequested=1;}
    buildPage(NoEvent);unsigned selected=stack.current().selected;
    if(currentInput.event==ModuleSelect){selected=currentInput.selected;repairSelection(menu.rows,selected);}
    else if(currentInput.event==ModuleUp||currentInput.event==ModuleDown)moveSelection(menu.rows,selected,currentInput.event==ModuleDown?1:-1);
    stack.select(selected);const int before=stack.current().page;
    if(currentInput.event==ModuleFavourite)toggleModelFavourite();
    const Event action=currentInput.event<=ModuleRight?static_cast<Event>(currentInput.event):NoEvent;
    if(action!=NoEvent){buildPage(action);if(menu.destination()>=0&&stack.current().page==before){stack.enter(menu.destination(),pages);wardrobe.clearConfirmation();}}
    if(pendingBack){pendingBack=false;stack.back();} // e.g. Loadout "Delete File" returns to the file list (Menyoo SetPreviousMenu)
    // NoEvent rebuilding refreshes localized/numeric values, never repeats A.
    buildPage(NoEvent);renderPage();
   }
   if(dirty){
    if(!configured||!saveAppearance(settingsPath,appearance)||!saveTheme(settingsPath,style))
     copy(produced.message,sizeof(produced.message),localized("Settings save pending.",appearance.language));
    else dirty=false;
   }
  }catch(const std::exception&error){produced.status=ModuleError;copy(produced.message,sizeof(produced.message),error.what());}
  catch(...){produced.status=ModuleError;copy(produced.message,sizeof(produced.message),"TRAINER_MODULE_EXCEPTION");}
  // WAIT is outside catch(...): cancellation must unwind, never be swallowed.
  runtime.scheduler().wait(0);
 }
}
static int attach(const Host*value){
 if(attached||executing||(owner&&owner!=GetCurrentThreadId()))return WrongContext;
 if(!value||value->size!=sizeof(*value)||value->version!=HostAbiVersion||!value->invoke)return BadAbi;
 host=*value;if(!stats.bind(host)||!registerPages())return ModuleError;characters.bind(host);pedActions.bind(host);wardrobe.bind(pedActions);overlays.bind(host);clones.bind(host);replenishment.bind(host);vehicles.bind(host);retained.bind(host);teleportActions.bind(host);teleportPages.bind(teleportActions);
 sn::binding().caller=sourceCaller;sn::binding().user=NULL;sn::binding().failures=0;tw::notifier()=showActionMessage;
 {const VehiclePageIds v={VehicleSpawnerPage,CustomsPage,VehWeaponsPage,VehMultipliersPage,VehNeonsPage,VehNeonColourPage,VehAutoDrivePage,VehSlamPage};vehicleIds=v;}
 {const CustomsPageIds c={CustomsPage,CustomsBennys,CustomsCatall,CustomsPaints,CustomsPaints2,CustomsChrome,CustomsClassic,CustomsMatte,CustomsMetallic,CustomsMetal,
   CustomsChameleon,CustomsPearl,CustomsUtil,CustomsWorn,CustomsShared,CustomsRgb,CustomsWheels,CustomsWheels2,CustomsWheels3,CustomsNeons,CustomsDoors,CustomsWindows,
   CustomsEmblem,CustomsRemovable,CustomsLights,CustomsEngineSound,CustomsEngineSoundItems};vehicleCustoms.bind(c);}
 {const SpawnerPageIds s={SpawnSettingsPage,FunnyPage,VehicleItemsPage,SpawnDlcPage,SpawnDlcItemsPage,CustomsPaints2,CustomsRgb};vehicleSpawner.bind(s,vehicles,vehicleCustoms);}
 {const WeaponPageIds w={WeaponIndivsPage,WeaponCategoryPage,WeaponItemPage,WeaponModsPage,WeaponParachutePage,WeaponSmokePage,WeaponFavouritesPage,
   WeaponLoadoutsPage,WeaponLoadoutPage,WeaponLaserPage,WeaponLaserColourPage};weaponIds=w;}
 pendingBack=false;
 owner=GetCurrentThreadId();session=0;attached=true;stack.reset(Main);pauseClock=syncClock=freezeWanted=false;frozenWanted=0;dirty=false;actionMessage[0]=0;actionMessageUntil=0;
 appearance=defaultAppearance();style=defaultStyle();configured=false;settingsPath[0]=0;return Success;
}
static int configure(const char*path){
 if(!attached||executing||session||owner!=GetCurrentThreadId())return WrongContext;
 if(!path||strlen(path)>=sizeof(settingsPath)||!((path[0]>='A'&&path[0]<='Z')||(path[0]>='a'&&path[0]<='z'))||
    path[1]!=':'||(path[2]!='\\'&&path[2]!='/'))return InvalidInput;
 strcpy_s(settingsPath,path);appearance=loadAppearance(path);style=loadTheme(path);configured=true;
 if(!favourites.configure(path))showActionMessage("Could not load favourites; existing data was preserved.");
 weaponOptions.configure(path);return Success;
}
static int tick(const Input2*input,Frame2*frame){
 if(!input||!frame||input->size!=sizeof(*input)||input->version!=ModuleAbiVersion3||frame->size!=sizeof(*frame)||frame->version!=ModuleAbiVersion3)return BadAbi;
 memset(frame,0,sizeof(*frame));frame->size=sizeof(*frame);frame->version=ModuleAbiVersion3;
 if(!attached||executing||owner!=GetCurrentThreadId())return WrongContext;
 if(input->permissions!=RequiredPermissions||!input->session||!input->generation||input->event>ModuleFavourite||input->visible>1||input->language>1||input->closeRequested>1)return InvalidInput;
 if(!session){
  if(!runtime.attach(input->session)||!runtime.resets().bindToggle(pauseClock,NULL)||!runtime.resets().bindToggle(freezeWanted,NULL)||
     !weather.bind(host,runtime.resets(),input->session)||!playerActions.bind(host,runtime.resets(),input->session)||
     !playerRoutine.bind(host,runtime.resets())||!abilities.bind(host,runtime.resets())||
     !legacyCheats.bind(host,runtime.resets(),input->session)||
     !vehicleOptions.bind(vehicleIds,runtime.resets())||!weaponOptions.bind(weaponIds,runtime.resets(),teleportActions,currentTimescale)||
     !runtime.bindCloseCleanup(releaseClonesOnClose,NULL)||!runtime.add(routine,NULL))return ModuleError;
  session=input->session;if(!configured)appearance.language=input->language;
 }
 currentInput=*input;memset(&produced,0,sizeof(produced));produced.size=sizeof(produced);produced.version=ModuleAbiVersion3;
 executing=true;stats.permit(true);weather.permit(true);characters.permit(true);pedActions.permit(true);overlays.permit(true);playerActions.permit(true);clones.permit(true);replenishment.permit(true);playerRoutine.permit(true);abilities.permit(true);vehicles.permit(true);vehicleOptions.permit(true);legacyCheats.permit(true);retained.permit(true);teleportActions.permit(true);
 int result=ModuleError;try{result=runtime.tick(*input);}catch(...){result=ModuleError;}
 stats.permit(false);weather.permit(false);characters.permit(false);pedActions.permit(false);overlays.permit(false);playerActions.permit(false);clones.permit(false);replenishment.permit(false);playerRoutine.permit(false);abilities.permit(false);vehicles.permit(false);vehicleOptions.permit(false);legacyCheats.permit(false);retained.permit(false);teleportActions.permit(false);executing=false;
 if(result==ModuleClosed&&!runtime.attached()){attached=false;session=0;host=Host();frame->status=ModuleClosed;return ModuleClosed;}
 if(result==Success&&!input->closeRequested){*frame=produced;return static_cast<int>(produced.status);}
 frame->status=result;return result;
}
static int close(){
 if(owner&&owner!=GetCurrentThreadId())return ModuleWrongOwner;
 if(executing)return ModuleNeedsOwnedTick;
 if(!attached)return ModuleClosed;
 if(!session){attached=false;host=Host();return ModuleClosed;}
 return runtime.close();
}
static const Module2 module={sizeof(module),ModuleAbiVersion3,attach,tick,close};
extern "C" __declspec(dllexport) const Module2*GtavxrMenyooModule2(uint32_t version){return version==ModuleAbiVersion3?&module:NULL;}
extern "C" __declspec(dllexport) int GtavxrMenyooConfigure2(const char*path){return configure(path);}
extern "C" __declspec(dllexport) int GtavxrMenyooPresentShared(const SharedMenu*input,DrawFrame*output){
 if(!attached||owner!=GetCurrentThreadId()||executing)return WrongContext;
 if(!input||!output||input->size!=sizeof(*input)||input->version!=SharedMenuVersion||
    !input->generation||!input->count||input->count>SharedMenuRows||input->count>input->total||
    input->offset>input->selected||input->selected-input->offset>=input->count||
    input->offset>input->total-input->count||!memchr(input->title,0,sizeof(input->title)))return InvalidInput;
 Transport shared;shared.language(appearance.language);shared.begin(Root,General,input->selected-input->offset,NoEvent);
 shared.setTitle(input->title);
 for(unsigned i=0;i<input->count;++i){const SharedMenuRow&row=input->rows[i];
  if(row.kind>BreakRow||row.enabled>1||row.submenu>1||!memchr(row.label,0,sizeof(row.label))||
     !memchr(row.value,0,sizeof(row.value))||!memchr(row.help,0,sizeof(row.help)))return InvalidInput;
  const size_t before=shared.rows.size();shared.add(row.label,static_cast<Kind>(row.kind),row.value,row.submenu?0:-1);
  if(shared.rows.size()!=before+1)return InvalidInput;
  shared.description(row.help);shared.rows.back().enabled=row.enabled!=0;
 }
 if(!selectable(shared.rows[input->selected-input->offset]))return InvalidInput;
 Presentation result=present(shared,style,input->selected-input->offset);
 result.selected=input->selected;result.total=input->total;result.offset=input->offset;
 // Native page window supplied by the accepted VR controller. The SAME fonts,
 // artwork/widgets/theme/localization plan handles it and the Trainer rows.
 char localCounter[48];sprintf_s(localCounter,"%u / %u",input->selected-input->offset+1,input->count);
 for(unsigned i=0;i<result.commands.size();++i)if(result.commands[i].text==localCounter&&
     result.commands[i].rightAligned&&result.commands[i].font==0&&result.commands[i].scale==.26f){
  char counter[48];sprintf_s(counter,"%u / %u",input->selected+1,input->total);result.commands[i].text=counter;}
 return encodePresentation(result,input->generation,*output)?Success:ModuleError;
}
