#pragma once
#include "host_abi.h"
#include "reset.h"
#include "menu_transport.h"
#include "source_player_routine_commands.h"
#include <string.h>
namespace menyoo_vr {
// Menyoo Routine.cpp: cover heals4, non-aiming only; Auto-Clean clears native
// blood/visible damage; Super Jump is a native THIS_FRAME request, not a new
// locomotion/tracking implementation. Reset stops routines, not past healing.
class PlayerRoutineActions {
 Host host_;bool permitted_,modes_[3],timed_;uint32_t lastTime_;uint64_t lastGeneration_;
 bool call(uint64_t op,const uint64_t*args,unsigned count,int*value=NULL){
  if(!permitted_||!host_.invoke)return false;uint64_t result[3]={};
  if(host_.invoke(host_.user,op,args,count,result)!=Success)return false;
  if(value)memcpy(value,result,sizeof(*value));return true;
 }
 bool actor(int&ped){return call(source_player_routine::PLAYER_PED_ID,NULL,0,&ped)&&ped>0;}
 bool current(int expected){int actual=0;return actor(actual)&&actual==expected;}
public:
 PlayerRoutineActions():permitted_(false),timed_(false),lastTime_(0),lastGeneration_(0){memset(&host_,0,sizeof(host_));memset(modes_,0,sizeof(modes_));}
 bool bind(const Host&host,ResetLedger&ledger){
  host_=host;permitted_=false;timed_=false;lastGeneration_=0;memset(modes_,0,sizeof(modes_));
  for(unsigned i=0;i<3;++i)if(!ledger.bindToggle(modes_[i],NULL))return false;return true;
 }
 void permit(bool enabled){permitted_=enabled;}
 // single rows so Player Options keeps the source order (Refill, ..., Auto-Clean, Super Run, Super Jump).
 void row(Transport&m,unsigned i){const char*names[]={"Refill Health When In Cover","Auto-Clean","Super Jump"};
  const char*help[]={"Gradually restores health while in cover and not aiming. Reset stops healing; it cannot undo restored health.",
   "Clears native blood and visible damage. Reset stops cleaning; it cannot restore past damage.",
   "Uses GTA's native super jump. Does not replace your VR movement or tracking controls."};
  if(i>2)return;if(m.add(names[i],ToggleRow,modes_[i]?"ON":"OFF")&&m.event()==Accept)modes_[i]=!modes_[i];m.description(help[i]);
 }
 void page(Transport&m){const char*names[]={"Refill Health When In Cover","Auto-Clean","Super Jump"};
  const char*help[]={"Gradually restores health while in cover and not aiming. Reset stops healing; it cannot undo restored health.",
   "Clears native blood and visible damage. Reset stops cleaning; it cannot restore past damage.",
   "Uses GTA's native super jump. Does not replace your VR movement or tracking controls."};
  for(unsigned i=0;i<3;++i){if(m.add(names[i],ToggleRow,modes_[i]?"ON":"OFF")&&m.event()==Accept)modes_[i]=!modes_[i];m.description(help[i]);}
 }
 bool tick(uint64_t generation){
  if(!permitted_||!generation)return false;if(lastGeneration_==generation)return true;lastGeneration_=generation;
  if(!modes_[0]&&!modes_[1]&&!modes_[2]){timed_=false;return true;}
  int ped=0;if(!actor(ped))return false;const uint64_t id=static_cast<uint32_t>(ped);
  if(modes_[2]){int player=-1;if(!call(source_player_routine::PLAYER_ID,NULL,0,&player)||player<0||!current(ped))return false;
   const uint64_t index=static_cast<uint32_t>(player);if(!call(source_player_routine::SET_SUPER_JUMP_THIS_FRAME,&index,1))return false;}
  if(!modes_[0]&&!modes_[1])return true;
  int rawTime=0;if(!call(source_player_routine::GET_GAME_TIMER,NULL,0,&rawTime))return false;
  const uint32_t now=static_cast<uint32_t>(rawTime);if(timed_&&static_cast<uint32_t>(now-lastTime_)<100u)return true;
  timed_=true;lastTime_=now;
  if(modes_[0]){int cover=0,aiming=0,health=0,maximum=0;const uint64_t coverArgs[2]={id,0};
   if(!call(source_player_routine::IS_PED_IN_COVER,coverArgs,2,&cover)||!call(source_player_routine::IS_PED_AIMING_FROM_COVER,&id,1,&aiming))return false;
   if(cover&&!aiming){if(!call(source_player_routine::GET_ENTITY_HEALTH,&id,1,&health)||
      !call(source_player_routine::GET_ENTITY_MAX_HEALTH,&id,1,&maximum)||maximum<=0)return false;
    if(health>0&&health<maximum){if(!current(ped))return false;
     const int amount=maximum-health<4?maximum:health+4;const uint64_t args[3]={id,static_cast<uint32_t>(amount),0};
     if(!call(source_player_routine::SET_ENTITY_HEALTH,args,3))return false;}}
  }
  if(modes_[1]){if(!current(ped)||!call(source_player_routine::CLEAR_PED_BLOOD_DAMAGE,&id,1)||
      !current(ped)||!call(source_player_routine::RESET_PED_VISIBLE_DAMAGE,&id,1))return false;}
  return true;
 }
 void suspend(){memset(modes_,0,sizeof(modes_));timed_=false;}
};
}
