#pragma once
#include "theme.h"
#include "menu_transport.h"
namespace menyoo_vr {
enum SharedSettingsRoute { SettingsLanguage=1,SettingsThemes,SettingsColours,SettingsResetToggles,SettingsResetAll };
struct SettingsPageResult {int route;bool changed;SettingsPageResult():route(0),changed(false){}};
inline SettingsPageResult sharedAppearancePage(Transport&menu,Appearance&appearance){
 SettingsPageResult result;menu.language(appearance.language);menu.setTitle("Settings");
 const bool selected=menu.add("Language",ChoiceRow,appearance.language==Spanish?"Spanish":"English",-1,0,Language);
 if(selected&&(menu.event()==Accept||menu.event()==Left||menu.event()==Right)){
  appearance.language=appearance.language==Spanish?English:Spanish;result.changed=true;
 }
 if(menu.add("Theme",ActionRow,"",SettingsThemes,0,Theme)&&menu.event()==Accept)result.route=SettingsThemes;
 if(menu.add("Colour Selector",ActionRow,"",SettingsColours,0,ColourSelector)&&menu.event()==Accept)result.route=SettingsColours;
 if(menu.add("Reset Toggles",ActionRow,"",-1,0,ResetToggles)&&menu.event()==Accept)result.route=SettingsResetToggles;
 menu.description("Disables ongoing toggle effects. Does not undo cash, stats, spawns or progress.");
 if(menu.add("Reset All Cheats",ActionRow,"",-1,0,ResetAllCheats)&&menu.event()==Accept)result.route=SettingsResetAll;
 menu.description("Also restores reversible values owned by this trainer. Other owners' changes are preserved.");
 return result;
}
inline bool sharedThemesPage(Transport&menu,Appearance&appearance,ThemeStyle&style){
 menu.language(appearance.language);menu.setTitle("Themes");bool changed=false;
 for(unsigned i=0;i<ThemeCount;++i){Appearance proposed=appearance;ThemeStyle candidate;
  if(!chooseTheme(i,proposed,candidate))continue;
  const bool selected=menu.add(OriginalThemes[i].name,ToggleRow,sameStyle(candidate,style)?"ON":"OFF");
  menu.icons(sameStyle(candidate,style),MakeupIcon,NoIcon);
  if(selected&&menu.event()==Accept){appearance=proposed;style=candidate;changed=true;}
 }
 return changed;
}
inline const char*themeColourName(unsigned role){
 const char*const names[]={"Header","Background","Title Text","Option Text","Selected Text","Separator Text","Option Counter","Selection","Ped Trackers"};
 return role<ThemeColourCount?names[role]:"";
}
inline bool sharedColourPage(Transport&menu,ThemeStyle&style,unsigned role){
 if(role>=ThemeColourCount||!validStyle(style))return false;
 menu.setTitle(themeColourName(role));uint32_t*channels[]={&style.colour[role].r,&style.colour[role].g,&style.colour[role].b,&style.colour[role].a};
 const char*const names[]={"Red","Green","Blue","Alpha"};bool changed=false;
 for(unsigned i=0;i<4;++i){char value[24];sprintf_s(value,"%u",*channels[i]);
  if(menu.add(names[i],NumberRow,value)&&(menu.event()==Left||menu.event()==Right)){
   const int direction=menu.event()==Right?1:-1;int next=static_cast<int>(*channels[i])+direction;
   if(next<0)next=0;if(next>255)next=255;if(*channels[i]!=static_cast<uint32_t>(next)){*channels[i]=next;changed=true;}
  }
 }
 return changed;
}
}
