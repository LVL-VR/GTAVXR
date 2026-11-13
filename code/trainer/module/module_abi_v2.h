#pragma once
#include "host_abi.h"
#include "presentation_abi.h"
namespace menyoo_vr {
enum { ModuleAbiVersion2=2 };
// v1 row events remain unchanged. Navigation belongs to the new shared stack.
enum ModuleInput2 { ModuleNoEvent=HostNoEvent,ModuleAccept=HostAccept,ModuleLeft=HostLeft,ModuleRight=HostRight,
 ModuleUp=4,ModuleDown=5,ModuleBack=6,ModuleSelect=7,ModuleOpenSettings=8,ModuleOpenTrainer=9 };
enum CloseStatus { ModuleClosed=0,ModuleNeedsOwnedTick=32,ModuleWrongOwner=33,ModuleLiveFiber=34,ModuleRestorePending=35 };
// v1's void detach cannot report a still-live suspended stack. Keep it as a
// historical prototype only; actual multi-page integration uses this contract.
struct Input2 {
 uint32_t size,version,permissions,event,selected,visible,language,closeRequested;
 uint64_t session,generation,milliseconds;
};
struct Frame2 {
 uint32_t size,version,status,language;
 DrawFrame drawing;
 char message[512];
 uint32_t page,hideRequested;
};
typedef int (*AttachModule2)(const Host*);
typedef int (*TickModule2)(const Input2*,Frame2*);
typedef int (*CloseModule2)(); // never unload unless ModuleClosed
struct Module2 {uint32_t size,version;AttachModule2 attach;TickModule2 tick;CloseModule2 close;};
typedef const Module2*(*GetModule2)(uint32_t);
typedef int (*ConfigureModule2)(const char*ownedSettingsPath);
}
