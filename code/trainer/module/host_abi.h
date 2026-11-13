#pragma once
#include <stdint.h>
// No std::string, STL, exceptions, allocator-owned pointers or compiler-specific
// C++ objects cross the native VS2012 <-> modern Menyoo module boundary.
namespace menyoo_vr {
enum { HostAbiVersion=1,FrameRows=12 };
enum Status { Success=0,BadAbi=1,WrongContext=2,MissingNative=3,InvalidInput=4,ModuleError=5,AlreadyUpdated=6 };
enum InputEvent { HostNoEvent=0,HostAccept=1,HostLeft=2,HostRight=3 };
enum HostPermission { OwnedStoryScript=1,GameUpdateThread=2,OfflineStory=4 };
enum { RequiredPermissions=OwnedStoryScript|GameUpdateThread|OfflineStory };
typedef int (*InvokeNative)(void*user,uint64_t sourceHash,const uint64_t*arguments,uint32_t count,uint64_t result[3]);
struct Host {uint32_t size,version;void*user;InvokeNative invoke;};
struct Input {uint32_t size,version,permissions,event,selected,offset,visible,reserved;uint64_t session,generation;};
struct AbiRow {char label[256],value[64],help[512];uint32_t kind;int32_t submenu;};
struct Frame {uint32_t size,version,rowCount,totalRows,selected,offset;char title[128];AbiRow rows[FrameRows];char error[256];};
typedef int (*AttachModule)(const Host*host);
typedef int (*TickModule)(const Input*input,Frame*frame);
typedef void (*DetachModule)();
struct Module {uint32_t size,version;AttachModule attach;TickModule tick;DetachModule detach;};
typedef const Module*(*GetModule)(uint32_t version);
}
