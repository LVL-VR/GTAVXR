#pragma once
#include "host_abi.h"
#include "reset.h"
#include "menu_transport.h"
#include "legacy_cheat_abi.h"
#include <string.h>
namespace menyoo_vr {
class LegacyCheatActions {
 struct Property{LegacyCheatActions*actions;unsigned id;};Host host_;ResetLedger*ledger_;uint64_t session_;bool permitted_;Property properties_[LegacyCheatCount];
 bool call(uint64_t op,const uint64_t*args,unsigned count,uint64_t&value){if(!permitted_||!host_.invoke)return false;uint64_t r[3]={};if(host_.invoke(host_.user,op,args,count,r)!=Success||r[0]>1)return false;value=r[0];return true;}
 static ResetAccess read(void*context,const OverrideOwner&owner,uint64_t&value){Property&p=*static_cast<Property*>(context);if(owner.session!=p.actions->session_)return ResetOwnerGone;uint64_t arg=p.id;return p.actions->call(SourceHostReadLegacyCheat,&arg,1,value)?ResetOk:ResetUnavailable;}
 static ResetAccess write(void*context,const OverrideOwner&owner,uint64_t value){Property&p=*static_cast<Property*>(context);if(owner.session!=p.actions->session_)return ResetOwnerGone;uint64_t args[2]={p.id,value},r=0;return p.actions->call(SourceHostWriteLegacyCheat,args,2,r)?ResetOk:ResetUnavailable;}
public:
 LegacyCheatActions():ledger_(NULL),session_(0),permitted_(false){memset(&host_,0,sizeof(host_));for(unsigned i=0;i<LegacyCheatCount;++i){properties_[i].actions=this;properties_[i].id=i;}}
 bool bind(const Host&host,ResetLedger&ledger,uint64_t session){if(!host.invoke||!session)return false;host_=host;ledger_=&ledger;session_=session;permitted_=false;return true;}
 void permit(bool enabled){permitted_=enabled;}
 bool set(unsigned id,uint64_t value){if(!ledger_||id>=LegacyCheatCount||value>1)return false;const OverrideOwner owner={session_,5,session_};
  if(value&&(id==LegacyFireAmmo||id==LegacyExplosiveAmmo)){const unsigned opposite=id==LegacyFireAmmo?LegacyExplosiveAmmo:LegacyFireAmmo;Property&other=properties_[opposite];
   if(ledger_->apply(500+opposite,owner,ToggleEffect,&other,read,write,0)!=ResetOk)return false;}
  Property&p=properties_[id];return ledger_->apply(500+id,owner,ToggleEffect,&p,read,write,value)==ResetOk;}
 bool row(Transport&m,unsigned id,const char*name){if(id>=LegacyCheatCount)return false;uint64_t value=0,arg=id;const bool known=call(SourceHostReadLegacyCheat,&arg,1,value);bool ok=true;
  if(m.add(name,ToggleRow,value?"ON":"OFF")&&m.event()==Accept&&known)ok=set(id,value?0:1);
  if(!known)m.rows.back().enabled=false;return ok;}
};
}
