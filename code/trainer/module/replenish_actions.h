#pragma once
#include "host_abi.h"
#include "source_replenish_commands.h"
#include <string.h>
namespace menyoo_vr {
// Original Menyoo one-shot health/armour/visual-damage action. No loop, WAIT,
// game-camera mutation or reset lease: restoring old damage is not a safe undo.
class ReplenishActions {
 Host host_;bool permitted_;
 bool call(uint64_t operation,const uint64_t*args,unsigned count,int*value=NULL){
  if(!permitted_||!host_.invoke)return false;uint64_t result[3]={};
  if(host_.invoke(host_.user,operation,args,count,result)!=Success)return false;
  if(value)memcpy(value,result,sizeof(*value));return true;
 }
public:
 ReplenishActions():permitted_(false){memset(&host_,0,sizeof(host_));}
 void bind(const Host&host){host_=host;permitted_=false;}void permit(bool enabled){permitted_=enabled;}
 bool apply(){
  int actor=0,player=-1,health=0,armour=0,current=0;
  if(!call(source_replenish::PLAYER_PED_ID,NULL,0,&actor)||actor<=0||
     !call(source_replenish::PLAYER_ID,NULL,0,&player)||player<0)return false;
  uint64_t ped=static_cast<uint32_t>(actor),playerIndex=static_cast<uint32_t>(player);
  if(!call(source_replenish::GET_ENTITY_MAX_HEALTH,&ped,1,&health)||health<=0||
     !call(source_replenish::GET_PLAYER_MAX_ARMOUR,&playerIndex,1,&armour)||armour<0||
     !call(source_replenish::PLAYER_PED_ID,NULL,0,&current)||current!=actor)return false;
  // This source has THREE arguments, not the old retail two-slot declaration.
  const uint64_t heal[3]={ped,static_cast<uint32_t>(health),0};
  const uint64_t protect[2]={ped,static_cast<uint32_t>(armour)};
  return call(source_replenish::SET_ENTITY_HEALTH,heal,3)&&
         call(source_replenish::SET_PED_ARMOUR,protect,2)&&
         call(source_replenish::CLEAR_PED_BLOOD_DAMAGE,&ped,1)&&
         call(source_replenish::RESET_PED_VISIBLE_DAMAGE,&ped,1);
 }
};
}
