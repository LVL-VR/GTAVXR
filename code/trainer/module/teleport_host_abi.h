#pragma once
#include <stdint.h>
namespace menyoo_vr {
static const uint64_t SourceHostTeleportRead=UINT64_C(0xfffffffffffff600);
static const uint64_t SourceHostTeleportCommit=UINT64_C(0xfffffffffffff601);
static const uint64_t SourceHostTeleportValidate=UINT64_C(0xfffffffffffff602);
// Synchronous UPDATE POD. No engine pointer survives or crosses the DLL ABI.
struct TeleportState {uint32_t size,vehicle,model,reserved;uint64_t epoch;float x,y,z,padding;};
}
