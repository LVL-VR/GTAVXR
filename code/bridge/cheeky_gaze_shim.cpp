// Built as CheekyOpenXRLayer.dll and placed in <game>\CheekyFoveatedDLSS\ for GTAVXR only. It is never an OpenXR
// layer: CheekyFoveatedDLSS's runtime looks up these exports on any module of that name, and its standalone overlay
// publishes the headset menu only while such a module is resident. The GTAVXR bridge owns the OpenXR session and
// publishes per-eye FOV, forward, gaze and DLSS-output identity through GtavxrCheeky_PublishGazeSnapshot.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include "cheeky_gaze_abi.h"

namespace {
SRWLOCK lock = SRWLOCK_INIT;
gx_cheeky_abi::Snapshot current{};
volatile LONG simulated = 0, pattern = 0, independent = 0;
}

extern "C" __declspec(dllexport) void __cdecl GtavxrCheeky_PublishGazeSnapshot(const gx_cheeky_abi::Snapshot* s) {
    if (!s || s->abi_version != gx_cheeky_abi::Version || s->structure_size != sizeof(*s)) return;
    AcquireSRWLockExclusive(&lock); std::memcpy(&current, s, sizeof(current)); ReleaseSRWLockExclusive(&lock);
}
// Returns 1 and a copy of the latest snapshot, 0 for an unsupported request (as Cheeky's own layer does).
extern "C" __declspec(dllexport) uint32_t __cdecl CheekyOpenXR_GetGazeSnapshot(uint32_t version, void* out, uint32_t size) {
    if (version != gx_cheeky_abi::Version || !out || size < sizeof(gx_cheeky_abi::Snapshot)) return 0;
    AcquireSRWLockShared(&lock); std::memcpy(out, &current, sizeof(current)); ReleaseSRWLockShared(&lock);
    if (!static_cast<gx_cheeky_abi::Snapshot*>(out)->abi_version) {   // nothing published yet
        auto* s = static_cast<gx_cheeky_abi::Snapshot*>(out);
        s->abi_version = gx_cheeky_abi::Version; s->structure_size = sizeof(*s);
    }
    return 1;
}
// Cheeky's simulation controls. The bridge never simulates gaze; the requests are recorded only.
extern "C" __declspec(dllexport) void __cdecl CheekyOpenXR_SetSimulatedGaze(uint32_t v) { InterlockedExchange(&simulated, LONG(v)); }
extern "C" __declspec(dllexport) void __cdecl CheekyOpenXR_SetSimulationPattern(uint32_t v) { InterlockedExchange(&pattern, LONG(v)); }
extern "C" __declspec(dllexport) void __cdecl CheekyOpenXR_SetIndependentGaze(uint32_t v) { InterlockedExchange(&independent, LONG(v)); }

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(module);
    return TRUE;
}
