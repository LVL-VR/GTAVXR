#pragma once
#include <stdint.h>
 
// these host operations travel through Host::invoke with an operation code in place of a native hash. No STL,
// allocator-owned pointer or C++ object crosses the VS2012 EXE <-> modern DLL boundary.
namespace gtavxr_script_host
{
enum : uint64_t
{
    // result[0] = address of script global <index> (0 when the index is outside the globals blocks). args: index.
    OpGlobalPtr = 0x4758524853000001ULL,
    // result[0] = count written. args: int* handles, int capacity. Script handles of the live pool entries.
    OpWorldPeds = 0x4758524853000002ULL,
    OpWorldVehicles = 0x4758524853000003ULL,
    OpWorldObjects = 0x4758524853000004ULL,
    OpWorldPickups = 0x4758524853000005ULL,
    // result[0] = entity address for a script handle, 0 when none. args: handle.
    OpEntityAddress = 0x4758524853000006ULL,
    // v10 textures (ScriptHookV createTexture / drawTexture). args: const void* DDS blob the DLL keeps alive until it releases the
    // request, (blob size | texture id << 32). The EXE decodes it into the txd "gtavxr_shv_<id>" holding one texture of the same
    // name, which DRAW_SPRITE draws. result[0] = request handle + 1 (0 = not issued), result[1] = the engine's request status.
    OpTextureDecode = 0x4758524853000007ULL,
    // result[0] = 2 ready, 1 decoding, 3 failed or unknown. args: handle + 1, 0.
    OpTextureStatus = 0x4758524853000008ULL,
    // releases the request and its txd (the engine frees them on a later update). args: handle + 1, 0.
    OpTextureRelease = 0x4758524853000009ULL,
};
inline bool isOperation(uint64_t code)
{
    return code >= OpGlobalPtr && code <= OpTextureRelease;
}
// ConfigureModule2 receives the profile's scripts folder (UTF-8). GtavxrScriptHostModule2 returns the Module2 table.
}
