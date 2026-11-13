// GTAVXR-owned NGX caller module, built as nvngx.dll_gtavxr.dll (P2 probe; later reused by the bridge).
// The DLSS5 NR model accepts NGX calls only from a module whose path contains "nvngx.dll" (interface fact
// observed in three independent integrations). This DLL owns those calls. Independent implementation.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ngx_iface.h"

namespace {
struct Model {
    HMODULE module, core;
    NgxInitExtFn init; NgxAllocateFn allocate; NgxDestroyFn destroy;
    NgxCreateFn create; NgxEvaluateFn evaluate; NgxReleaseFn release; NgxShutdownFn shutdown;
} g = {};
unsigned long long g_appId = 0; wchar_t g_dataPath[MAX_PATH] = {}; ID3D12Device* g_device = nullptr;
wchar_t g_corePath[MAX_PATH] = {}; int g_coreInit = 0, g_coreFirstAlloc = 0;
uint32_t g_feature = kNgxNeuralRenderingFeature;   // one model per forwarder copy: 18 = NR (default), 1 = DLSS SR

template<class F> F Export(HMODULE m, const char* name) { return m ? reinterpret_cast<F>(GetProcAddress(m, name)) : nullptr; }

// Each NGX call lives in its own small function so a structured exception inside the model is reported
// instead of terminating the host (__try cannot share a frame with objects that need unwinding).
int SafeInit(unsigned long long id, const wchar_t* path, ID3D12Device* d) {
    __try { volatile int r = g.init(id, path, d, kNgxSdkVersion, nullptr); return r; } __except (EXCEPTION_EXECUTE_HANDLER) { return GX_FWD_EXCEPTION; }
}
int SafeAllocate(NgxParameter** p) {
    __try { volatile int r = g.allocate(p); return r; } __except (EXCEPTION_EXECUTE_HANDLER) { return GX_FWD_EXCEPTION; }
}
int SafeDestroy(NgxParameter* p) {
    __try { volatile int r = g.destroy(p); return r; } __except (EXCEPTION_EXECUTE_HANDLER) { return GX_FWD_EXCEPTION; }
}
int SafeCreate(ID3D12GraphicsCommandList* l, NgxParameter* p, NgxHandle** h) {
    __try { volatile int r = g.create(l, g_feature, p, h); return r; } __except (EXCEPTION_EXECUTE_HANDLER) { return GX_FWD_EXCEPTION; }
}
int SafeEvaluate(ID3D12GraphicsCommandList* l, NgxHandle* h, NgxParameter* p) {
    __try { volatile int r = g.evaluate(l, h, p, nullptr); return r; } __except (EXCEPTION_EXECUTE_HANDLER) { return GX_FWD_EXCEPTION; }
}
int SafeRelease(NgxHandle* h) {
    __try { volatile int r = g.release(h); return r; } __except (EXCEPTION_EXECUTE_HANDLER) { return GX_FWD_EXCEPTION; }
}
int SafeShutdown(ID3D12Device* d) {
    __try { volatile int r = g.shutdown(d); return r; } __except (EXCEPTION_EXECUTE_HANDLER) { return GX_FWD_EXCEPTION; }
}
int SafeSetU32(NgxParameter* p, const char* n, unsigned int v) {
    __try { p->Set(n, v); return 0; } __except (EXCEPTION_EXECUTE_HANDLER) { return GX_FWD_EXCEPTION; }
}
int SafeSetF32(NgxParameter* p, const char* n, float v) {
    __try { p->Set(n, v); return 0; } __except (EXCEPTION_EXECUTE_HANDLER) { return GX_FWD_EXCEPTION; }
}
int SafeSetRes(NgxParameter* p, const char* n, ID3D12Resource* v) {
    __try { p->Set(n, v); return 0; } __except (EXCEPTION_EXECUTE_HANDLER) { return GX_FWD_EXCEPTION; }
}
int SafeGetF32(NgxParameter* p, const char* n, float* v) {
    __try { volatile int r = p->Get(n, v); return r; } __except (EXCEPTION_EXECUTE_HANDLER) { return GX_FWD_EXCEPTION; }
}
int SafeGetU32(NgxParameter* p, const char* n, unsigned int* v) {
    __try { volatile int r = p->Get(n, v); return r; } __except (EXCEPTION_EXECUTE_HANDLER) { return GX_FWD_EXCEPTION; }
}
}

extern "C" {
__declspec(dllexport) int gx_nr_load(const wchar_t* model_path, GxFwdExports* out) {
    if (!model_path || !out) return GX_FWD_BAD_ARGUMENT;
    if (!g.module) {
        g.module = LoadLibraryExW(model_path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!g.module) return static_cast<int>(GetLastError());
        g.init = Export<NgxInitExtFn>(g.module, "NVSDK_NGX_D3D12_Init_Ext");
        g.allocate = Export<NgxAllocateFn>(g.module, "NVSDK_NGX_D3D12_AllocateParameters");
        g.destroy = Export<NgxDestroyFn>(g.module, "NVSDK_NGX_D3D12_DestroyParameters");
        g.create = Export<NgxCreateFn>(g.module, "NVSDK_NGX_D3D12_CreateFeature");
        g.evaluate = Export<NgxEvaluateFn>(g.module, "NVSDK_NGX_D3D12_EvaluateFeature");
        g.release = Export<NgxReleaseFn>(g.module, "NVSDK_NGX_D3D12_ReleaseFeature");
        g.shutdown = Export<NgxShutdownFn>(g.module, "NVSDK_NGX_D3D12_Shutdown1");
    }
    out->init = g.init != nullptr; out->allocate = g.allocate != nullptr; out->destroy = g.destroy != nullptr;
    out->create = g.create != nullptr; out->evaluate = g.evaluate != nullptr; out->release = g.release != nullptr;
    out->shutdown = g.shutdown != nullptr; out->allocate_from_core = 0;
    return 0;
}
__declspec(dllexport) int gx_nr_init(unsigned long long app_id, const wchar_t* data_path, ID3D12Device* device) {
    if (!g.module) return GX_FWD_NOT_LOADED;
    if (!g.init) return GX_FWD_NO_EXPORT;
    g_appId = app_id; g_device = device; if (data_path) lstrcpynW(g_dataPath, data_path, MAX_PATH);
    return SafeInit(app_id, data_path, device);
}
// Parameter blocks come from the model module; the 310.8 NR model does not export the allocator (P2 first run),
// so they come from the driver's NGX core: an _nvngx.dll already in the process, else the one the driver
// registers at HKLM\SOFTWARE\NVIDIA Corporation\Global\NGXCore\FullPath. If the core refuses to allocate before
// its own Init, it is initialized once with the same application id, data path and device.
__declspec(dllexport) int gx_nr_alloc(void** parameters) {
    if (!g.module) return GX_FWD_NOT_LOADED;
    if (!parameters) return GX_FWD_BAD_ARGUMENT;
    if (!g.allocate || !g.destroy) {
        g.core = GetModuleHandleW(L"_nvngx.dll");
        if (g.core) GetModuleFileNameW(g.core, g_corePath, MAX_PATH);
        else {
            wchar_t dir[MAX_PATH] = {}; DWORD bytes = sizeof(dir);
            if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\NVIDIA Corporation\\Global\\NGXCore", L"FullPath", RRF_RT_REG_SZ, nullptr, dir, &bytes) != ERROR_SUCCESS) return GX_FWD_NO_EXPORT;
            wsprintfW(g_corePath, L"%s\\_nvngx.dll", dir);
            g.core = LoadLibraryExW(g_corePath, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        }
        g.allocate = Export<NgxAllocateFn>(g.core, "NVSDK_NGX_D3D12_AllocateParameters");
        g.destroy = Export<NgxDestroyFn>(g.core, "NVSDK_NGX_D3D12_DestroyParameters");
        if (!g.allocate || !g.destroy) return GX_FWD_NO_EXPORT;
        NgxParameter* probe = nullptr;
        g_coreFirstAlloc = SafeAllocate(&probe);
        if (NgxOk(g_coreFirstAlloc) && probe) SafeDestroy(probe);
        else {
            NgxInitExtFn coreInit = Export<NgxInitExtFn>(g.core, "NVSDK_NGX_D3D12_Init_Ext");
            if (!coreInit) return GX_FWD_NO_EXPORT;
            NgxInitExtFn modelInit = g.init; g.init = coreInit;
            g_coreInit = SafeInit(g_appId, g_dataPath, g_device);
            g.init = modelInit;
        }
    }
    NgxParameter* p = nullptr;
    const int r = SafeAllocate(&p);
    *parameters = p;
    return r;
}
__declspec(dllexport) int gx_nr_core_used() { return g.core != nullptr; }
__declspec(dllexport) void gx_nr_set_feature(unsigned int feature) { g_feature = feature; }
// Diagnostics for the probe: core path, first allocation result and (if needed) the core Init result.
__declspec(dllexport) void gx_nr_core_info(wchar_t* path, unsigned path_chars, int* first_alloc, int* core_init) {
    if (path && path_chars) lstrcpynW(path, g_corePath, static_cast<int>(path_chars));
    if (first_alloc) *first_alloc = g_coreFirstAlloc;
    if (core_init) *core_init = g_coreInit;
}
__declspec(dllexport) int gx_nr_destroy(void* parameters) {
    if (!g.destroy || !parameters) return GX_FWD_BAD_ARGUMENT;
    return SafeDestroy(static_cast<NgxParameter*>(parameters));
}
__declspec(dllexport) int gx_nr_set_u32(void* p, const char* n, unsigned int v) { return p && n ? SafeSetU32(static_cast<NgxParameter*>(p), n, v) : GX_FWD_BAD_ARGUMENT; }
__declspec(dllexport) int gx_nr_set_f32(void* p, const char* n, float v) { return p && n ? SafeSetF32(static_cast<NgxParameter*>(p), n, v) : GX_FWD_BAD_ARGUMENT; }
__declspec(dllexport) int gx_nr_set_res(void* p, const char* n, ID3D12Resource* v) { return p && n ? SafeSetRes(static_cast<NgxParameter*>(p), n, v) : GX_FWD_BAD_ARGUMENT; }
__declspec(dllexport) int gx_nr_get_f32(void* p, const char* n, float* v) { return p && n && v ? SafeGetF32(static_cast<NgxParameter*>(p), n, v) : GX_FWD_BAD_ARGUMENT; }
__declspec(dllexport) int gx_nr_get_u32(void* p, const char* n, unsigned int* v) { return p && n && v ? SafeGetU32(static_cast<NgxParameter*>(p), n, v) : GX_FWD_BAD_ARGUMENT; }
__declspec(dllexport) int gx_nr_create(ID3D12GraphicsCommandList* list, void* parameters, void** handle) {
    if (!g.create) return GX_FWD_NO_EXPORT;
    if (!list || !parameters || !handle) return GX_FWD_BAD_ARGUMENT;
    NgxHandle* h = nullptr;
    const int r = SafeCreate(list, static_cast<NgxParameter*>(parameters), &h);
    *handle = h;
    return r;
}
__declspec(dllexport) int gx_nr_evaluate(ID3D12GraphicsCommandList* list, void* handle, void* parameters) {
    if (!g.evaluate) return GX_FWD_NO_EXPORT;
    if (!list || !handle || !parameters) return GX_FWD_BAD_ARGUMENT;
    return SafeEvaluate(list, static_cast<NgxHandle*>(handle), static_cast<NgxParameter*>(parameters));
}
__declspec(dllexport) int gx_nr_release(void* handle) {
    if (!g.release) return GX_FWD_NO_EXPORT;
    return handle ? SafeRelease(static_cast<NgxHandle*>(handle)) : GX_FWD_BAD_ARGUMENT;
}
__declspec(dllexport) int gx_nr_shutdown(ID3D12Device* device) {
    if (!g.shutdown) return GX_FWD_NO_EXPORT;
    return device ? SafeShutdown(device) : GX_FWD_BAD_ARGUMENT;
}
}
