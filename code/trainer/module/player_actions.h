#pragma once
#include "host_abi.h"
#include "reset.h"
#include "menu_transport.h"
#include "player_property_abi.h"
#include <string.h>
namespace menyoo_vr {
class PlayerActions {
 struct Property{PlayerActions*actions;unsigned index;};
 Host host_;ResetLedger*ledger_;uint64_t session_;bool permitted_;Property properties_[PlayerPropertyCount];
 bool call(uint64_t operation,const uint64_t*args,unsigned count,uint64_t&value){
  if(!permitted_||!host_.invoke)return false;uint64_t result[3]={};
  if(host_.invoke(host_.user,operation,args,count,result)!=Success)return false;value=result[0];return true;
 }
 static ResetAccess read(void*context,const OverrideOwner&owner,uint64_t&value){
  Property&p=*static_cast<Property*>(context);PlayerActions&a=*p.actions;uint64_t epoch=0;
  if(owner.session!=a.session_)return ResetOwnerGone;
  if(!a.call(SourceHostPlayerEpoch,NULL,0,epoch))return ResetUnavailable;
  if(epoch!=owner.identity)return ResetOwnerGone;
  return a.call(SourceHostReadPlayerProperty+p.index,&epoch,1,value)?ResetOk:ResetUnavailable;
 }
 static ResetAccess write(void*context,const OverrideOwner&owner,uint64_t value){
  Property&p=*static_cast<Property*>(context);PlayerActions&a=*p.actions;uint64_t epoch=0,result=0;
  if(owner.session!=a.session_)return ResetOwnerGone;
  if(!a.call(SourceHostPlayerEpoch,NULL,0,epoch))return ResetUnavailable;
  if(epoch!=owner.identity)return ResetOwnerGone;
  uint64_t args[2]={epoch,value};return a.call(SourceHostWritePlayerProperty+p.index,args,2,result)?ResetOk:ResetUnavailable;
 }
public:
 PlayerActions():ledger_(NULL),session_(0),permitted_(false){memset(&host_,0,sizeof(host_));
  for(unsigned i=0;i<PlayerPropertyCount;++i){properties_[i].actions=this;properties_[i].index=i;}}
 bool bind(const Host&host,ResetLedger&ledger,uint64_t session){if(!host.invoke||!session)return false;
  host_=host;ledger_=&ledger;session_=session;permitted_=false;return true;}
 void permit(bool allowed){permitted_=allowed;}
 bool get(unsigned property,uint64_t&value,uint64_t&epoch){
  return property<PlayerPropertyCount&&call(SourceHostPlayerEpoch,NULL,0,epoch)&&epoch&&call(SourceHostReadPlayerProperty+property,&epoch,1,value);
 }
 bool set(unsigned property,uint64_t epoch,bool enabled){
  if(!ledger_||property>=PlayerPropertyCount||!epoch)return false;
  ledger_->reapGone();
  const OverrideOwner owner={session_,epoch,epoch};Property&p=properties_[property];
  return ledger_->apply(200+property,owner,ToggleEffect,&p,read,write,enabled?playerEnabledValue(property):playerDisabledValue(property))==ResetOk;
 }
 bool page(Transport&m){
  const char*names[]={"Invincibility","Invisibility","No Ragdoll","Seatbelt"};
  const char*help[]={"Ignores damage, drowning and the native damage proofs.","Makes your native character invisible.","Disables native ragdoll and ped-impact activation.","Prevents native vehicle ejection. Reset restores the captured knock-off and windscreen settings."};
  bool ok=true;
  for(unsigned i=0;i<PlayerPropertyCount;++i){uint64_t value=0,epoch=0;const bool known=get(i,value,epoch);
   const bool enabled=playerPropertyEnabled(i,value);
   if(m.add(names[i],ToggleRow,enabled?"ON":"OFF")&&m.event()==Accept&&known)ok=set(i,epoch,!enabled)&&ok;
   m.description(help[i]);if(!known)m.rows.back().enabled=false;
  }return ok;
 }
};
}
