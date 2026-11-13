#pragma once
#include <windows.h>
#include <stdint.h>
#include <stdexcept>
namespace menyoo_vr {
// ScriptHook-style cooperative WAIT, hosted INSIDE our owned native script's
// update. No OS worker thread, Sleep, Present hook or extra game update.
class FiberScheduler {
 struct Cancelled {};
 struct Job {FiberScheduler*owner;void*fiber;void(*entry)(void*);void*argument;uint64_t dueGeneration,dueTime;bool done,failed;
  Job():owner(NULL),fiber(NULL),entry(NULL),argument(NULL),dueGeneration(0),dueTime(0),done(false),failed(false){}};
 Job jobs_[2];unsigned count_;DWORD thread_;void*root_;Job*current_;uint64_t generation_,time_;bool cancelled_;
 static VOID WINAPI run(void*argument){Job&job=*static_cast<Job*>(argument);FiberScheduler&scheduler=*job.owner;
  try{if(scheduler.cancelled_)throw Cancelled();job.entry(job.argument);}catch(const Cancelled&){}catch(...){job.failed=true;}
  job.done=true;
  // Returning from a Windows fiber terminates the thread. Park until the owner
  // reclaims this FINISHED fiber from its root instead.
  for(;;)SwitchToFiber(scheduler.root_);
 }
 bool enter(bool&converted){
  converted=false;if(thread_&&thread_!=GetCurrentThreadId())return false;
  if(!thread_)thread_=GetCurrentThreadId();
  if(IsThreadAFiber())root_=GetCurrentFiber();else{root_=ConvertThreadToFiberEx(NULL,FIBER_FLAG_FLOAT_SWITCH);converted=root_!=NULL;}
  return root_!=NULL;
 }
 void leave(bool converted){root_=NULL;if(converted&&!ConvertFiberToThread())throw std::runtime_error("MENYOO_FIBER_RESTORE");}
 FiberScheduler(const FiberScheduler&);FiberScheduler&operator=(const FiberScheduler&);
public:
 FiberScheduler():count_(0),thread_(0),root_(NULL),current_(NULL),generation_(0),time_(0),cancelled_(false){}
 // Explicit close required while module code, callbacks and native context are
 // still alive. No destructor silently deletes suspended C++ stacks.
 ~FiberScheduler(){for(unsigned i=0;i<count_;++i)if(jobs_[i].fiber)abort();}
 bool add(void(*entry)(void*),void*argument){
  if(!entry||count_>=2||generation_||cancelled_||(thread_&&thread_!=GetCurrentThreadId()))return false;
  Job&job=jobs_[count_];job.owner=this;job.entry=entry;job.argument=argument;
  job.fiber=CreateFiberEx(64*1024,1024*1024,FIBER_FLAG_FLOAT_SWITCH,run,&job);
  if(!job.fiber)return false;++count_;return true;
 }
 bool tick(uint64_t generation,uint64_t now){
  if(cancelled_||current_||!generation||generation<=generation_||now<time_)return false;
  bool converted=false;if(!enter(converted))return false;generation_=generation;time_=now;
  for(unsigned i=0;i<count_;++i){Job&job=jobs_[i];if(job.done||generation<job.dueGeneration||now<job.dueTime)continue;
   current_=&job;SwitchToFiber(job.fiber);current_=NULL;}
  leave(converted);return true;
 }
 void wait(DWORD milliseconds){
  if(!current_||GetCurrentThreadId()!=thread_||!root_||GetCurrentFiber()!=current_->fiber)throw std::runtime_error("MENYOO_WAIT_CONTEXT");
  if(cancelled_)throw Cancelled();
  current_->dueGeneration=generation_==UINT64_MAX?UINT64_MAX:generation_+1;
  current_->dueTime=time_>UINT64_MAX-milliseconds?UINT64_MAX:time_+milliseconds;
  SwitchToFiber(root_);
  if(cancelled_)throw Cancelled();
 }
 bool close(){
  if(current_)return false;if(!count_)return true;
  bool converted=false;if(!enter(converted))return false;cancelled_=true;
  for(unsigned i=0;i<count_;++i){Job&job=jobs_[i];if(!job.done){current_=&job;SwitchToFiber(job.fiber);current_=NULL;}
   // If a callback caught cancellation and continued, do not free its stack or
   // unload its DLL. Preserve ownership and report the exact lifecycle failure.
   if(!job.done){leave(converted);return false;}
  }
  for(unsigned i=0;i<count_;++i){DeleteFiber(jobs_[i].fiber);jobs_[i].fiber=NULL;}
  count_=0;leave(converted);return true;
 }
 bool failed()const{for(unsigned i=0;i<count_;++i)if(jobs_[i].failed)return true;return false;}
 bool restart(){
  if(count_||current_||root_||(thread_&&thread_!=GetCurrentThreadId()))return false;
  thread_=0;generation_=time_=0;cancelled_=false;for(unsigned i=0;i<2;++i)jobs_[i]=Job();return true;
 }
};
}
