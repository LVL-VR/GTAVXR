#pragma once
#include <windows.h>
 
// launcher on the PC where the game runs. Order: the launcher's explicit variable (the Suite passes all three), else the
// package layout of GTAVXR_BRIDGE_PATH (CMD launchers), else nothing: user play NEVER falls back to the developer root
// (GX_DEV_ROOT); the caller names the missing path. Developer runs (no GTAVXR_USER_PLAY=1) keep the exact developer strings.
// Paths are read with the ANSI API like every consumer; the Suite refuses a data folder the ANSI code page cannot spell.
#include "../../../core/core/paths.h"

namespace gx_user_paths
{

enum Kind
{
    LOADER = 0,
    VR_SETTINGS = 1,
    SESSIONS = 2,
    KIND_COUNT = 3
};

inline const char* Variable(unsigned k)
{
    return k == LOADER        ? "GTAVXR_LOADER_PATH"
           : k == VR_SETTINGS ? "GTAVXR_VR_SETTINGS"
           : k == SESSIONS    ? "GTAVXR_SESSIONS_ROOT"
                              : "";
}
// the package-layout / developer relative path (the build164 strings)
inline const char* Relative(unsigned k)
{
    return k == LOADER        ? "local-cache/native-openxr-20260926/openxr_loader.dll"
           : k == VR_SETTINGS ? "local-cache/play/vr-settings.ini"
           : k == SESSIONS    ? "local-cache/play/sessions/"
                              : "";
}
// the separator each consumer always used: '/' for files, '\\' for the sessions prefix compared with GetFullPathNameA
inline char Sep(unsigned k)
{
    return k == SESSIONS ? '\\' : '/';
}

inline bool UserPlay()
{
    char mode[8] = {};
    const DWORD n = GetEnvironmentVariableA("GTAVXR_USER_PLAY", mode, sizeof(mode));
    return n && n < sizeof(mode) && mode[0] == '1' && !mode[1];
}

// out = the path of `k` for this process; false (out = "") when user play has neither the variable nor the package layout,
// or when a value does not fit (a truncated variable is refused, never shortened).
inline bool Resolve(unsigned k, char* out, unsigned size)
{
    if (!out || !size)
        return false;
    out[0] = 0;
    if (k >= KIND_COUNT)
        return false;
    char given[400] = {}, bridge[512] = {};
    const DWORD g = GetEnvironmentVariableA(Variable(k), given, sizeof(given));
    if (g >= sizeof(given))
        return false;
    if (GetEnvironmentVariableA("GTAVXR_BRIDGE_PATH", bridge, sizeof(bridge)) >= sizeof(bridge))
        bridge[0] = 0;
    return gx_paths::Resolve(given, bridge, UserPlay(), Relative(k), Sep(k), k == SESSIONS, out, size);
}

}
