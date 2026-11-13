#pragma once
#include "module_abi_v3.h"
#include "fiber_scheduler.h"
#include "reset.h"
namespace menyoo_vr {
class ModuleRuntime {
 FiberScheduler scheduler_;ResetLedger resets_;DWORD owner_;
 uint64_t session_,generation_,milliseconds_;bool attached_,insideTick_,closing_,fibersClosed_;
 bool(*closeCleanup_)(void*);void*closeCleanupContext_;
 ModuleRuntime(const ModuleRuntime&);ModuleRuntime&operator=(const ModuleRuntime&);
public:
 ModuleRuntime():owner_(0),session_(0),generation_(0),milliseconds_(0),attached_(false),insideTick_(false),closing_(false),fibersClosed_(false),closeCleanup_(NULL),closeCleanupContext_(NULL){}
 ResetLedger&resets(){return resets_;}
 FiberScheduler&scheduler(){return scheduler_;}
 bool attach(uint64_t session){
  if(!session||attached_||insideTick_||(owner_&&owner_!=GetCurrentThreadId()))return false;
  if(!scheduler_.restart()||!resets_.releaseBindings())return false;
  owner_=GetCurrentThreadId();session_=session;generation_=milliseconds_=0;attached_=true;closing_=fibersClosed_=false;closeCleanup_=NULL;closeCleanupContext_=NULL;return true;
 }
 bool bindCloseCleanup(bool(*cleanup)(void*),void*context){if(!attached_||generation_||insideTick_||!cleanup)return false;closeCleanup_=cleanup;closeCleanupContext_=context;return true;}
 bool add(void(*routine)(void*),void*argument){return attached_&&!generation_&&owner_==GetCurrentThreadId()&&scheduler_.add(routine,argument);}
 int tick(const Input2&input){
  if(!attached_||insideTick_||owner_!=GetCurrentThreadId())return WrongContext;
  if(input.size!=sizeof(input)||input.version!=ModuleAbiVersion3||input.permissions!=RequiredPermissions||
     input.session!=session_||!input.generation||input.event>ModuleFavourite||input.visible>1||input.language>1||input.closeRequested>1||input.milliseconds<milliseconds_)return InvalidInput;
  if(input.generation<=generation_)return AlreadyUpdated;
  generation_=input.generation;milliseconds_=input.milliseconds;insideTick_=true;
  struct TickExit {bool*flag;explicit TickExit(bool&value):flag(&value){}~TickExit(){*flag=false;}}exit(insideTick_);
  if(input.closeRequested)closing_=true;
  if(closing_){
   if(!fibersClosed_){if(!scheduler_.close())return ModuleLiveFiber;fibersClosed_=true;}
   if(closeCleanup_&&!closeCleanup_(closeCleanupContext_))return ModuleRestorePending;
   const ResetReport restored=resets_.reset(true);
   if(!restored.settled()||resets_.activeLeases())return ModuleRestorePending;
   attached_=false;return ModuleClosed;
  }
  return scheduler_.tick(input.generation,input.milliseconds)&&!scheduler_.failed()?Success:ModuleError;
 }
 int close(){
  if(owner_&&owner_!=GetCurrentThreadId())return ModuleWrongOwner;
  if(!attached_&&!insideTick_)return ModuleClosed;
  closing_=true;return ModuleNeedsOwnedTick;
 }
 bool attached()const{return attached_;}
};
}
