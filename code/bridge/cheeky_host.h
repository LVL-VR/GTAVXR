#pragma once
// Optional CheekyFoveatedDLSS standalone host, started by the bridge instead of a dxgi/version proxy: the Suite moves
// every top-level proxy DLL out of the game folder for each session (RetailPlugins), but leaves subfolders alone.
// Opt-in by presence of <game exe folder>\CheekyFoveatedDLSS\CheekyFoveatedDLSSHost.dll (GTAVXR_CHEEKY_DIR overrides
// the folder, GTAVXR_CHEEKY=0 disables). Same contract as Cheeky's bootstrap loader: load the host from its own folder
// plus System32 only, call CheekyHost_Start(1 = standalone) outside any DllMain. It runs before the eye chain loads
// nvngx_dlss.dll, so Cheeky's NGX hooks are in place for GTAVXR's per-eye DLSS calls. Once per process; the host is
// pinned by itself and never unloaded.
// The same folder holds GTAVXR's CheekyOpenXRLayer.dll shim (cheeky_gaze_shim.cpp), loaded first: it is how Cheeky
// receives per-eye FOV/gaze (CheekyOpenXR_GetGazeSnapshot) and why its overlay publishes the headset menu.
#include <windows.h>
#include <string>
#include <vector>
#include <cstring>
#include <openxr/openxr.h>
#include "cheeky_gaze_abi.h"

namespace gx_cheeky {
inline std::string narrow(const std::wstring& w) {
    std::string out; for (wchar_t c : w) out += c < 128 ? char(c) : '?'; return out;
}
// The Cheeky folder when the host is installed and not disabled, empty otherwise. Resolved once.
inline const std::wstring& folder() {
    static const std::wstring value = [] {
        wchar_t flag[8] = {};
        if (GetEnvironmentVariableW(L"GTAVXR_CHEEKY", flag, 8) && flag[0] == L'0') return std::wstring();
        wchar_t dir[MAX_PATH] = {};
        std::wstring f;
        DWORD n = GetEnvironmentVariableW(L"GTAVXR_CHEEKY_DIR", dir, MAX_PATH);
        if (n && n < MAX_PATH) f = dir;
        else {
            n = GetModuleFileNameW(nullptr, dir, MAX_PATH);
            if (!n || n >= MAX_PATH) return std::wstring();
            f.assign(dir); f = f.substr(0, f.find_last_of(L"\\/")) + L"\\CheekyFoveatedDLSS";
        }
        return GetFileAttributesW((f + L"\\CheekyFoveatedDLSSHost.dll").c_str()) == INVALID_FILE_ATTRIBUTES ? std::wstring() : f;
    }();
    return value;
}
// Whether the instance this process created asked for XR_EXT_eye_gaze_interaction (both creation paths record it).
inline bool& gaze_extension_enabled() { static bool v = false; return v; }
// Instance extensions for GTAVXR: D3D11, plus eye gaze only with Cheeky installed and the runtime advertising it
// (PSVR2 through PSVR2Toolkit's SteamVR driver does). Without Cheeky nothing changes.
template <class Api> std::vector<const char*> instance_extensions(Api& api) {
    std::vector<const char*> out{"XR_KHR_D3D11_enable"}; // XR_KHR_D3D11_ENABLE_EXTENSION_NAME
    gaze_extension_enabled() = false;
    if (folder().empty() || !api.EnumerateInstanceExtensionProperties) return out;
    uint32_t n = 0;
    if (XR_FAILED(api.EnumerateInstanceExtensionProperties(nullptr, 0, &n, nullptr)) || !n || n > 512) return out;
    std::vector<XrExtensionProperties> ext(n, {XR_TYPE_EXTENSION_PROPERTIES});
    if (XR_FAILED(api.EnumerateInstanceExtensionProperties(nullptr, n, &n, ext.data()))) return out;
    for (uint32_t i = 0; i < n; ++i)
        if (!strcmp(ext[i].extensionName, XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME)) {
            out.push_back(XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME); gaze_extension_enabled() = true; break;
        }
    return out;
}
// The shim's publisher, once start_once loaded it.
inline gx_cheeky_abi::PublishFn& publisher() { static gx_cheeky_abi::PublishFn p = nullptr; return p; }
// Returns a log line on the first call in the process that found a host, empty otherwise.
inline std::string start_once() {
    static bool attempted = false;
    if (attempted) return {};
    attempted = true;
    if (folder().empty()) return {};
    const std::wstring host = folder() + L"\\CheekyFoveatedDLSSHost.dll", shim = folder() + L"\\CheekyOpenXRLayer.dll";
    std::string menu = " shim=absent";
    if (GetFileAttributesW(shim.c_str()) != INVALID_FILE_ATTRIBUTES) {
        if (HMODULE m = LoadLibraryExW(shim.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32)) {
            publisher() = reinterpret_cast<gx_cheeky_abi::PublishFn>(GetProcAddress(m, "GtavxrCheeky_PublishGazeSnapshot"));
            menu = publisher() ? " shim=gtavxr" : " shim=foreign (not GTAVXR's; gaze/FOV unavailable)";
        } else menu = " shim=load_failed win32=" + std::to_string(GetLastError());
    }
    HMODULE module = LoadLibraryExW(host.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) return "CHEEKY load_failed win32=" + std::to_string(GetLastError()) + " path=" + narrow(host);
    using StartFn = bool (*)(unsigned);
    const auto start = reinterpret_cast<StartFn>(GetProcAddress(module, "CheekyHost_Start"));
    if (!start) return "CHEEKY start_export_missing path=" + narrow(host);
    const bool ok = start(1);
    return std::string("CHEEKY host_start=") + (ok ? "ok" : "failed (see CheekyFoveatedDLSS-Host.log)") + menu +
        " gaze_ext=" + (gaze_extension_enabled() ? "1" : "0") + " path=" + narrow(host);
}
}
