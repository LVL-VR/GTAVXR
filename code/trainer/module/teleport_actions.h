#pragma once
#include "host_abi.h"
#include "teleport_host_abi.h"
#include "source_teleport_commands.h"
#include <math.h>
#include <float.h>
#include <string.h>
namespace menyoo_vr {
class TeleportActions {
 Host host_;bool permitted_;
 bool call(uint64_t op,const uint64_t*args,unsigned count,uint64_t result[3]){
  return permitted_&&host_.invoke&&host_.invoke(host_.user,op,args,count,result)==Success;
 }
 static uint64_t bits(float value){uint64_t result=0;memcpy(&result,&value,4);return result;}
 static float number(uint64_t bitsValue){float value=0;memcpy(&value,&bitsValue,4);return value;}
 bool entity(int&ped,int&target){uint64_t r[3]={};
  if(!call(source_teleport::PLAYER_PED_ID,NULL,0,r)||!r[0]||r[0]>0x7fffffff)return false;ped=target=static_cast<int>(r[0]);
  uint64_t args[2]={static_cast<unsigned>(ped),0};
  if(!call(source_teleport::GET_VEHICLE_PED_IS_IN,args,2,r)||r[0]>0x7fffffff)return false;
  if(r[0])target=static_cast<int>(r[0]);return true;
 }
public:
 TeleportActions():permitted_(false){memset(&host_,0,sizeof(host_));}
 void bind(const Host&host){host_=host;permitted_=false;}void permit(bool value){permitted_=value;}
 bool current(TeleportState&state){state=TeleportState();state.size=sizeof(state);uint64_t r[3]={},arg=reinterpret_cast<uintptr_t>(&state);
  return call(SourceHostTeleportRead,&arg,1,r)&&state.size==sizeof(state)&&state.epoch&&finite(state.x,state.y,state.z);
 }
 static bool finite(float x,float y,float z){return _finite(x)&&_finite(y)&&_finite(z);}
 bool coordinates(float x,float y,float z){
  if(!finite(x,y,z))return false;TeleportState state={};if(!current(state))return false;
  int ped=0,target=0;if(!entity(ped,target))return false;
  state.x=x;state.y=y;state.z=z;uint64_t r[3]={},proposal=reinterpret_cast<uintptr_t>(&state);
  if(!call(SourceHostTeleportValidate,&proposal,1,r))return false;
  uint64_t args[8]={static_cast<unsigned>(target),bits(x),bits(y),bits(z),0,0,0,1};
  if(!call(source_teleport::SET_ENTITY_COORDS,args,8,r))return false;
  // The native host checks the actual resulting transform and actor epoch,
  // then re-seeds the common eye/hand/HUD room reference. A void native's
  // dispatch alone is NOT evidence of a completed teleport.
  const uint64_t commit=reinterpret_cast<uintptr_t>(&state);return call(SourceHostTeleportCommit,&commit,1,r);
 }
 bool forward(){TeleportState state={};if(!current(state))return false;int ped=0,target=0;if(!entity(ped,target))return false;
  const uint64_t args[4]={static_cast<unsigned>(ped),0,bits(3.5f),0};uint64_t r[3]={};
  return call(source_teleport::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS,args,4,r)&&coordinates(number(r[0]),number(r[1]),number(r[2]));
 }
};
}
