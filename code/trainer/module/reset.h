#pragma once
#include <stdint.h>
#include <stddef.h>
namespace menyoo_vr {
// Typed engine adapters own reads/writes and validate live entity identity.
// No addresses, pointers to retired actors, default-value guesses or memcpy
// into RAGE objects live in this ledger.
enum ResetAccess { ResetOk,ResetUnavailable,ResetOwnerGone,ResetConflict };
enum OverrideScope { ToggleEffect,ExplicitCheatOverride };
struct OverrideOwner {uint64_t session,identity,epoch;};
inline bool sameOwner(const OverrideOwner&a,const OverrideOwner&b){return a.session==b.session&&a.identity==b.identity&&a.epoch==b.epoch;}
typedef ResetAccess (*ReadOverride)(void*,const OverrideOwner&,uint64_t&);
typedef ResetAccess (*WriteOverride)(void*,const OverrideOwner&,uint64_t);
typedef ResetAccess (*DisableToggle)(void*);
struct ResetReport {
 unsigned disabled,restored,conflicts,pending,ownerGone;
 ResetReport():disabled(0),restored(0),conflicts(0),pending(0),ownerGone(0){}
 bool complete()const{return !conflicts&&!pending;}
 bool settled()const{return !pending;}
};
class ResetLedger {
 struct Toggle {bool*enabled;void*context;DisableToggle disable;bool pending;};
 struct Lease {uint32_t key;OverrideOwner owner;OverrideScope scope;void*context;
  ReadOverride read;WriteOverride write;uint64_t original,applied;bool held;};
 Toggle toggles_[128];Lease leases_[128];unsigned toggleCount_,leaseCount_;bool resetting_;
 ResetLedger(const ResetLedger&);ResetLedger&operator=(const ResetLedger&);
 void disableAll(ResetReport&report){
  for(unsigned i=0;i<toggleCount_;++i){Toggle&t=toggles_[i];
   if(!*t.enabled&&!t.pending)continue;
   if(*t.enabled){*t.enabled=false;++report.disabled;} // stop reapplication FIRST
   const ResetAccess result=t.disable?t.disable(t.context):ResetOk;
   t.pending=result==ResetUnavailable;
   if(result==ResetOwnerGone)++report.ownerGone;
   else if(result==ResetConflict)++report.conflicts;
   else if(result!=ResetOk)++report.pending;
  }
 }
 void restore(bool all,ResetReport&report){
  for(unsigned i=0;i<leaseCount_;++i){Lease&lease=leases_[i];
   if(!lease.held||(!all&&lease.scope!=ToggleEffect))continue;
   uint64_t current=0;ResetAccess result=lease.read(lease.context,lease.owner,current);
   if(result==ResetOwnerGone){lease.held=false;++report.ownerGone;continue;}
   if(result!=ResetOk){if(result==ResetConflict)++report.conflicts;else ++report.pending;continue;}
   if(current==lease.original){lease.held=false;continue;}
   // Another owner modified the setting: retain the lease for reporting but
   // never overwrite that owner's current value during a blanket reset.
   if(current!=lease.applied){lease.held=false;++report.conflicts;continue;}
   result=lease.write(lease.context,lease.owner,lease.original);
   if(result==ResetOk){lease.held=false;++report.restored;}
   else if(result==ResetOwnerGone){lease.held=false;++report.ownerGone;}
   else if(result==ResetConflict){lease.held=false;++report.conflicts;}else ++report.pending;
  }
 }
public:
 ResetLedger():toggleCount_(0),leaseCount_(0),resetting_(false){}
 bool bindToggle(bool&enabled,void*context,DisableToggle disable=NULL){
  if(resetting_||toggleCount_==128)return false;
  for(unsigned i=0;i<toggleCount_;++i)if(toggles_[i].enabled==&enabled)return false;
  Toggle&t=toggles_[toggleCount_++];t.enabled=&enabled;t.context=context;t.disable=disable;t.pending=false;return true;
 }
 ResetAccess apply(uint32_t key,const OverrideOwner&owner,OverrideScope scope,void*context,
                   ReadOverride read,WriteOverride write,uint64_t value){
  if(resetting_||!key||!owner.session||!owner.identity||!owner.epoch||!read||!write)return ResetUnavailable;
  if(!canApply(key,owner))return ResetUnavailable; // reject capacity before engine mutation
  Lease*lease=NULL;
  for(unsigned i=0;i<leaseCount_;++i)if(leases_[i].key==key&&sameOwner(leases_[i].owner,owner)){lease=&leases_[i];break;}
  if(lease&&lease->held&&(lease->context!=context||lease->read!=read||lease->write!=write||lease->scope!=scope))return ResetConflict;
  uint64_t current=0;const ResetAccess queried=read(context,owner,current);if(queried!=ResetOk)return queried;
  if(lease&&lease->held&&current!=lease->applied)return ResetConflict;
  const ResetAccess result=write(context,owner,value);if(result!=ResetOk)return result;
  if(!lease){
   for(unsigned i=0;i<leaseCount_;++i)if(!leases_[i].held){lease=&leases_[i];break;}
   if(!lease){lease=&leases_[leaseCount_++];lease->held=false;}
  }
  if(!lease->held)lease->original=current;
  lease->key=key;lease->owner=owner;lease->scope=scope;lease->context=context;
  lease->read=read;lease->write=write;lease->applied=value;lease->held=true;return ResetOk;
 }
 bool canApply(uint32_t key,const OverrideOwner&owner)const{
  for(unsigned i=0;i<leaseCount_;++i)if(!leases_[i].held||(leases_[i].key==key&&sameOwner(leases_[i].owner,owner)))return true;
  return leaseCount_<128;
 }
 ResetReport reset(bool all){
  ResetReport report;if(resetting_){report.pending=1;return report;}
  resetting_=true;disableAll(report);restore(all,report);resetting_=false;return report;
 }
 unsigned activeLeases()const{unsigned count=0;for(unsigned i=0;i<leaseCount_;++i)if(leases_[i].held)++count;return count;}
 // Reuse stale actor slots ONLY after their typed read proves OwnerGone.
 // Unavailable/conflicting/live owners are retained; no engine mutation here.
 unsigned reapGone(){
  if(resetting_)return 0;unsigned released=0;
  for(unsigned i=0;i<leaseCount_;++i)if(leases_[i].held){Lease&lease=leases_[i];uint64_t value=0;
   if(lease.read(lease.context,lease.owner,value)==ResetOwnerGone){lease.held=false;++released;}}
  return released;
 }
 bool releaseBindings(){
  if(resetting_||activeLeases())return false;
  for(unsigned i=0;i<toggleCount_;++i)if(toggles_[i].pending||*toggles_[i].enabled)return false;
  toggleCount_=leaseCount_=0;return true;
 }
 // Cash/stats/inventory/spawns/teleports/progress have NO registration path in
 // this ledger. Reset All Cheats cannot pretend to undo these one-shot actions.
};
}
