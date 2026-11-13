#pragma once
#include "menu_transport.h"
#include <stdint.h>
#include <limits.h>
namespace menyoo_vr {
// Menyoo StatManager's exact Story prefixes/key. Numeric controller editing
// replaces its text box, not the native stat or protagonist identity.
inline const char*storyPrefix(unsigned character){static const char*const values[]={"SP0_","SP1_","SP2_"};return character<3?values[character]:NULL;}
inline const char*storyName(unsigned character){static const char*const values[]={"Michael","Franklin","Trevor"};return character<3?values[character]:NULL;}
inline int cashStep(unsigned step){static const int values[]={1,10,100,1000,10000,100000,1000000};return step<7?values[step]:0;}
template<class Stats>bool editStoryCash(Stats&stats,unsigned character,unsigned step,int direction,int&displayed){
 const char*prefix=storyPrefix(character);const int amount=cashStep(step);
 if(!prefix||!amount||(direction!=-1&&direction!=0&&direction!=1))return false;
 const std::string key=std::string(prefix)+"TOTAL_CASH";int current=0;
 if(!stats.getInt(key,current)||current<0)return false;
 if(!direction){displayed=current;return true;}
 int64_t value=static_cast<int64_t>(current)+static_cast<int64_t>(direction)*amount;
 if(value<0)value=0;if(value>INT_MAX)value=INT_MAX;
 if(value!=current&&!stats.setInt(key,static_cast<int>(value)))return false;
 displayed=static_cast<int>(value);return true;
}
template<class Stats>void storyCashPage(Transport&menu,Stats&stats,unsigned character,unsigned&step){
 int cash=0;menu.setTitle(storyName(character)?storyName(character):"Basic Stats");
 if(!editStoryCash(stats,character,step,0,cash)){
  const size_t before=menu.rows.size();menu.add("Total Cash",NumberRow,"Unavailable");
  if(menu.rows.size()>before)menu.rows.back().enabled=false;return;
 }
 char value[48];sprintf_s(value,"$%d",cash);
 const bool selected=menu.add("Total Cash",NumberRow,value);
 menu.description("Changes this protagonist's cash. Resets do not undo it.");
 if(selected&&(menu.event()==Left||menu.event()==Right)){
  if(editStoryCash(stats,character,step,menu.event()==Right?1:-1,cash)){
   sprintf_s(value,"$%d",cash);menu.rows.back().value=value;
  }else menu.description("Cash change failed; the previous value was preserved.");
 }
 sprintf_s(value,"%d",cashStep(step));const bool stepSelected=menu.add("Step Amount",ChoiceRow,value,-1,0,NumericEditorStep);
 if(stepSelected&&menu.event()==Right)step=(step+1)%7;
 else if(stepSelected&&menu.event()==Left)step=(step+6)%7;
}
}
