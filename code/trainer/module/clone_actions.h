#pragma once
#include "host_abi.h"
#include "source_ped_commands.h"
#include "source_stat_commands.h"
#include "source_clone_commands.h"
#include "clone_host_abi.h"
#include <string.h>
#include <vector>
namespace menyoo_vr {
enum CloneKind { CloneNeutral,CloneCompanion,CloneEnemy };
class CloneActions {
 Host host_;bool permitted_;std::vector<int>ownedClones_;
 bool call(uint64_t hash,const uint64_t*args,unsigned count,uint64_t&value){
  if(!permitted_||!host_.invoke)return false;uint64_t result[3]={};
  if(host_.invoke(host_.user,hash,args,count,result)!=Success)return false;value=result[0];return true;
 }
 static uint64_t ptr(const void*value){return reinterpret_cast<uintptr_t>(value);}
public:
 CloneActions():permitted_(false){memset(&host_,0,sizeof(host_));}
 void bind(const Host&host){host_=host;permitted_=false;ownedClones_.clear();}void permit(bool allowed){permitted_=allowed;}
 bool release(){
  if(!permitted_)return false;std::vector<int>pending;
  for(unsigned i=0;i<ownedClones_.size();++i){uint64_t actor=static_cast<uint32_t>(ownedClones_[i]),value=0;
   // Host validates native GUID salt AND current owned script handler before
   // releasing. A foreign/reused/missing entity settles without mutating it.
   if(!call(SourceHostReleaseOwnedClone,&actor,1,value)||!value)pending.push_back(ownedClones_[i]);
  }ownedClones_.swap(pending);return ownedClones_.empty();
 }
 bool clone(unsigned kind,int&created){
  created=0;if(kind>CloneEnemy||ownedClones_.size()>=64)return false;uint64_t value=0;
  if(!call(source_ped::PLAYER_PED_ID,NULL,0,value)||static_cast<int>(value)<=0)return false;const uint64_t player=value;
  int group=0;
  if(kind==CloneCompanion){
   // The player ALREADY owns a native permanent group. Creating a new group
   // and making the player its leader asserts in this exact source.
   uint64_t playerIndex=0;
   if(!call(source_clone::PLAYER_ID,NULL,0,playerIndex))return false;
   if(!call(source_clone::GET_PLAYER_GROUP,&playerIndex,1,value))return false;group=static_cast<int>(value);
   int leader=0,followers=0;uint64_t query[3]={static_cast<uint32_t>(group),ptr(&leader),ptr(&followers)};
   if(!call(source_clone::GET_GROUP_SIZE,query,3,value)||followers>=7)return false;
  }
  uint64_t args[6]={player,0,1,1,0,0};
  if(!call(source_clone::CLONE_PED,args,4,value)||static_cast<int>(value)<=0)return false;
  created=static_cast<int>(value);ownedClones_.push_back(created);const uint64_t actor=static_cast<uint32_t>(created);
  uint64_t heading=0;if(!call(source_clone::GET_ENTITY_HEADING,&player,1,heading))return false;
  args[0]=actor;args[1]=heading;if(!call(source_clone::SET_ENTITY_HEADING,args,2,value))return false;
  args[1]=1;if(!call(source_clone::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS,args,2,value))return false;
  args[1]=2; // actual CCombatData::CA_Professional, not original100/200/net ID
  if(!call(source_clone::SET_PED_COMBAT_ABILITY,args,2,value))return false;
  if(kind==CloneNeutral){
   if(!call(SourceHostReleaseOwnedClone,&actor,1,value)||!value)return false;ownedClones_.pop_back();return true;}
  const char*weapon="WEAPON_APPISTOL";uint64_t address=ptr(weapon);
  if(!call(source_stats::GET_HASH_KEY,&address,1,value))return false;const uint64_t weaponHash=value;
  args[0]=actor;args[1]=weaponHash;args[2]=9999;args[3]=1;
  if(!call(source_clone::GIVE_DELAYED_WEAPON_TO_PED,args,4,value))return false;
  args[2]=1;if(!call(source_clone::SET_CURRENT_PED_WEAPON,args,3,value))return false;
  // Original Menyoo enum FullAuto=0xC6EE6B4C, rather than guessing a name hash.
  args[0]=actor;args[1]=UINT64_C(0xc6ee6b4c);if(!call(source_clone::SET_PED_FIRING_PATTERN,args,2,value))return false;
  args[1]=100;if(!call(source_clone::SET_PED_SHOOT_RATE,args,2,value))return false;
  if(kind==CloneCompanion){args[1]=static_cast<uint32_t>(group);return call(source_clone::SET_PED_AS_GROUP_MEMBER,args,2,value);}
  args[1]=player;args[2]=0;args[3]=16;
  if(!call(source_clone::TASK_COMBAT_PED,args,4,value))return false;
  // Use the original initial native combat task, kept alive. Its AI already
  // chooses gun/melee/range behavior. Original redundant sequence calls repeat
  // AFTER close (asserts in this source), and clearing an open sequence also
  // asserts/leaves the global active slot. No sequence/global mutation needed.
  args[0]=actor;args[1]=1;return call(source_clone::SET_PED_KEEP_TASK,args,2,value);
 }
};
}
