#pragma once
#include "host_abi.h"
#include "source_ped_commands.h"
#include "trainer_world_abi.h"
#include <string.h>
#include <vector>
namespace menyoo_vr {
class PedActions {
 Host host_;bool permitted_;
 bool call(uint64_t hash,const uint64_t*arguments,unsigned count,int*value=NULL){
  if(!permitted_||!host_.invoke)return false;uint64_t result[3]={};
  if(host_.invoke(host_.user,hash,arguments,count,result)!=Success)return false;
  if(value)memcpy(value,result,sizeof(*value));return true;
 }
 bool id(int&ped){return call(source_ped::PLAYER_PED_ID,NULL,0,&ped)&&ped>0;}
public:
 PedActions():permitted_(false){memset(&host_,0,sizeof(host_));}
 void bind(const Host&host){host_=host;permitted_=false;}void permit(bool allowed){permitted_=allowed;}
 bool query(uint64_t operation,unsigned index,int&value,int drawable=-1){
  int ped=0;if(!id(ped)||index>=12)return false;uint64_t args[3]={static_cast<uint32_t>(ped),index,static_cast<uint32_t>(drawable)};
  return call(operation,args,drawable>=0?3u:2u,&value);
 }
 bool component(unsigned index,int drawable,int texture,int palette){
  int ped=0;if(index>=12||drawable<0||texture<0||palette<0||palette>3||!id(ped))return false;
  uint64_t args[5]={static_cast<uint32_t>(ped),index,static_cast<uint32_t>(drawable),static_cast<uint32_t>(texture),static_cast<uint32_t>(palette)};
  return call(source_ped::SET_PED_COMPONENT_VARIATION,args,5);
 }
 bool prop(unsigned index,int drawable,int texture){
  int ped=0;if(index>=10||drawable< -1||texture<0||!id(ped))return false;
  uint64_t args[5]={static_cast<uint32_t>(ped),index,static_cast<uint32_t>(drawable),static_cast<uint32_t>(texture),0};
  return drawable<0?call(source_ped::CLEAR_PED_PROP,args,2):call(source_ped::SET_PED_PROP_INDEX,args,5);
 }
 // Collection / Local ID data (typed host operation SourceHostVariationInfo).
 bool collections(bool isProp,unsigned slot,std::vector<VariationCollection>&out){
  out.clear();int ped=0;if(slot>=(isProp?10u:12u)||!id(ped))return false;out.resize(64);
  VariationQuery q={};q.size=sizeof(q);q.ped=ped;q.isProp=isProp?1:0;q.slot=static_cast<int>(slot);q.capacity=64;q.collections=out.data();
  const uint64_t arg=reinterpret_cast<uintptr_t>(&q);
  if(!call(SourceHostVariationInfo,&arg,1)||q.status!=VariationOk||q.count>q.capacity){out.clear();return false;}
  out.resize(q.count);for(size_t i=0;i<out.size();++i)out[i].name[sizeof(out[i].name)-1]=0;return true;
 }
 bool utility(unsigned action){
  int ped=0;if(action>4||!id(ped))return false;uint64_t args[2]={static_cast<uint32_t>(ped),0};
  if(action==0)return call(source_ped::SET_PED_RANDOM_COMPONENT_VARIATION,args,2);
  if(action==1||action==2){if(!call(source_ped::SET_PED_DEFAULT_COMPONENT_VARIATION,args,1))return false;
   return action==1||call(source_ped::CLEAR_ALL_PED_PROPS,args,1);}
  return call(action==3?source_ped::SET_PED_RANDOM_PROPS:source_ped::CLEAR_ALL_PED_PROPS,args,1);
 }
};
}
