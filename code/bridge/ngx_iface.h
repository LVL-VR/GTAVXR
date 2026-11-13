#pragma once
// GTAVXR-owned declaration of the NGX parameter interface and entry points used for DLSS5 Neural Rendering.
// Interface facts only (names, argument order, feature number), written from the public NGX parameter
// contract and confirmed against three independent callers read for the study (OFXR, Cheeky, Vice City VR);
// no third-party code is copied. MSVC lays the overloads out the same way as NVIDIA's own MSVC-built header.
#include <stdint.h>
struct ID3D11Resource;
struct ID3D12Resource;
struct ID3D12Device;
struct ID3D12GraphicsCommandList;

typedef int NgxResult;   // NVSDK_NGX_Result: 0x1 success, 0xBAD0xxxx failure
inline bool NgxOk(NgxResult r) { return (static_cast<uint32_t>(r) & 0xFFF00000u) != 0xBAD00000u; }

struct NgxParameter {
    virtual void Set(const char*, unsigned long long) = 0;
    virtual void Set(const char*, float) = 0;
    virtual void Set(const char*, double) = 0;
    virtual void Set(const char*, unsigned int) = 0;
    virtual void Set(const char*, int) = 0;
    virtual void Set(const char*, ID3D11Resource*) = 0;
    virtual void Set(const char*, ID3D12Resource*) = 0;
    virtual void Set(const char*, void*) = 0;
    virtual NgxResult Get(const char*, unsigned long long*) const = 0;
    virtual NgxResult Get(const char*, float*) const = 0;
    virtual NgxResult Get(const char*, double*) const = 0;
    virtual NgxResult Get(const char*, unsigned int*) const = 0;
    virtual NgxResult Get(const char*, int*) const = 0;
    virtual NgxResult Get(const char*, ID3D11Resource**) const = 0;
    virtual NgxResult Get(const char*, ID3D12Resource**) const = 0;
    virtual NgxResult Get(const char*, void**) const = 0;
    virtual void Reset() = 0;
};
struct NgxHandle;

typedef NgxResult (*NgxInitExtFn)(unsigned long long, const wchar_t*, ID3D12Device*, uint32_t, const NgxParameter*);
typedef NgxResult (*NgxAllocateFn)(NgxParameter**);
typedef NgxResult (*NgxDestroyFn)(NgxParameter*);
typedef NgxResult (*NgxCreateFn)(ID3D12GraphicsCommandList*, uint32_t, NgxParameter*, NgxHandle**);
typedef NgxResult (*NgxEvaluateFn)(ID3D12GraphicsCommandList*, const NgxHandle*, const NgxParameter*, void*);
typedef NgxResult (*NgxReleaseFn)(NgxHandle*);
typedef NgxResult (*NgxShutdownFn)(ID3D12Device*);

static const uint32_t kNgxNeuralRenderingFeature = 18;
static const uint32_t kNgxSdkVersion = 0x15;

// Forwarder ABI (nvngx.dll_gtavxr.dll). The NR model only accepts calls whose caller module path contains
// "nvngx.dll", so every NGX call and every parameter write is made from inside the forwarder.
// Return values: NGX results, or the GX_FWD_* codes below.
#define GX_FWD_NOT_LOADED   ((int)0xF0000001)
#define GX_FWD_NO_EXPORT    ((int)0xF0000002)
#define GX_FWD_EXCEPTION    ((int)0xF0000003)
#define GX_FWD_BAD_ARGUMENT ((int)0xF0000004)
struct GxFwdExports {           // which entry points the model module itself exports
    uint32_t init, allocate, destroy, create, evaluate, release, shutdown, allocate_from_core;
};
typedef int  (*GxFwdLoadFn)(const wchar_t* model_path, GxFwdExports* out);
typedef int  (*GxFwdInitFn)(unsigned long long app_id, const wchar_t* data_path, ID3D12Device* device);
typedef int  (*GxFwdAllocFn)(void** parameters);
typedef int  (*GxFwdDestroyFn)(void* parameters);
typedef int  (*GxFwdSetU32Fn)(void* parameters, const char* name, unsigned int value);
typedef int  (*GxFwdSetF32Fn)(void* parameters, const char* name, float value);
typedef int  (*GxFwdSetResFn)(void* parameters, const char* name, ID3D12Resource* value);
typedef int  (*GxFwdGetF32Fn)(void* parameters, const char* name, float* value);
typedef int  (*GxFwdGetU32Fn)(void* parameters, const char* name, unsigned int* value);
typedef int  (*GxFwdCreateFn)(ID3D12GraphicsCommandList* list, void* parameters, void** handle);
typedef int  (*GxFwdEvaluateFn)(ID3D12GraphicsCommandList* list, void* handle, void* parameters);
typedef int  (*GxFwdReleaseFn)(void* handle);
typedef int  (*GxFwdShutdownFn)(ID3D12Device* device);
