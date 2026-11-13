#pragma once
struct OpenXrApi {
    HMODULE module = nullptr;
    PFN_xrGetInstanceProcAddr GetInstanceProcAddr = nullptr;
    PFN_xrEnumerateInstanceExtensionProperties EnumerateInstanceExtensionProperties = nullptr;
    PFN_xrCreateInstance CreateInstance = nullptr;
    PFN_xrDestroyInstance DestroyInstance = nullptr;
    PFN_xrGetInstanceProperties GetInstanceProperties = nullptr;
    PFN_xrGetSystem GetSystem = nullptr;
    PFN_xrGetSystemProperties GetSystemProperties = nullptr;
    PFN_xrEnumerateViewConfigurations EnumerateViewConfigurations = nullptr;
    PFN_xrEnumerateViewConfigurationViews EnumerateViewConfigurationViews = nullptr;
    PFN_xrEnumerateEnvironmentBlendModes EnumerateEnvironmentBlendModes = nullptr;
    PFN_xrGetD3D11GraphicsRequirementsKHR GetD3D11GraphicsRequirementsKHR = nullptr;
    PFN_xrCreateSession CreateSession = nullptr;
    PFN_xrDestroySession DestroySession = nullptr;
    PFN_xrPollEvent PollEvent = nullptr;
    PFN_xrBeginSession BeginSession = nullptr;
    PFN_xrEndSession EndSession = nullptr;
    PFN_xrRequestExitSession RequestExitSession = nullptr;
    PFN_xrEnumerateReferenceSpaces EnumerateReferenceSpaces = nullptr;
    PFN_xrCreateReferenceSpace CreateReferenceSpace = nullptr;
    PFN_xrDestroySpace DestroySpace = nullptr;
    PFN_xrEnumerateSwapchainFormats EnumerateSwapchainFormats = nullptr;
    PFN_xrCreateSwapchain CreateSwapchain = nullptr;
    PFN_xrDestroySwapchain DestroySwapchain = nullptr;
    PFN_xrEnumerateSwapchainImages EnumerateSwapchainImages = nullptr;
    PFN_xrAcquireSwapchainImage AcquireSwapchainImage = nullptr;
    PFN_xrWaitSwapchainImage WaitSwapchainImage = nullptr;
    PFN_xrReleaseSwapchainImage ReleaseSwapchainImage = nullptr;
    PFN_xrWaitFrame WaitFrame = nullptr;
    PFN_xrBeginFrame BeginFrame = nullptr;
    PFN_xrEndFrame EndFrame = nullptr;
    PFN_xrLocateViews LocateViews = nullptr;
    PFN_xrResultToString ResultToString = nullptr;

    template <typename T>
    bool LoadGlobal(const char* name, T& target) {
        PFN_xrVoidFunction raw = nullptr;
        const XrResult result = GetInstanceProcAddr(XR_NULL_HANDLE, name, &raw);
        target = reinterpret_cast<T>(raw);
        return XR_SUCCEEDED(result) && target != nullptr;
    }

    template <typename T>
    bool LoadInstance(XrInstance instance, const char* name, T& target) {
        PFN_xrVoidFunction raw = nullptr;
        const XrResult result = GetInstanceProcAddr(instance, name, &raw);
        target = reinterpret_cast<T>(raw);
        return XR_SUCCEEDED(result) && target != nullptr;
    }

    bool Load(const std::wstring& path) {
        module = LoadLibraryW(path.empty() ? L"openxr_loader.dll" : path.c_str());
        if (!module) return false;
        GetInstanceProcAddr = reinterpret_cast<PFN_xrGetInstanceProcAddr>(
            GetProcAddress(module, "xrGetInstanceProcAddr"));
        if (!GetInstanceProcAddr) return false;
        return LoadGlobal("xrEnumerateInstanceExtensionProperties",
                          EnumerateInstanceExtensionProperties) &&
               LoadGlobal("xrCreateInstance", CreateInstance);
    }

    bool LoadForInstance(XrInstance instance) {
#define LOAD_XR(name) LoadInstance(instance, "xr" #name, name)
        return LOAD_XR(DestroyInstance) && LOAD_XR(GetInstanceProperties) &&
               LOAD_XR(GetSystem) && LOAD_XR(GetSystemProperties) &&
               LOAD_XR(EnumerateViewConfigurations) &&
               LOAD_XR(EnumerateViewConfigurationViews) &&
               LOAD_XR(EnumerateEnvironmentBlendModes) &&
               LOAD_XR(GetD3D11GraphicsRequirementsKHR) &&
               LOAD_XR(CreateSession) && LOAD_XR(DestroySession) &&
               LOAD_XR(PollEvent) && LOAD_XR(BeginSession) &&
               LOAD_XR(EndSession) && LOAD_XR(RequestExitSession) &&
               LOAD_XR(EnumerateReferenceSpaces) &&
               LOAD_XR(CreateReferenceSpace) && LOAD_XR(DestroySpace) &&
               LOAD_XR(EnumerateSwapchainFormats) && LOAD_XR(CreateSwapchain) &&
               LOAD_XR(DestroySwapchain) && LOAD_XR(EnumerateSwapchainImages) &&
               LOAD_XR(AcquireSwapchainImage) && LOAD_XR(WaitSwapchainImage) &&
               LOAD_XR(ReleaseSwapchainImage) && LOAD_XR(WaitFrame) &&
               LOAD_XR(BeginFrame) && LOAD_XR(EndFrame) &&
               LOAD_XR(LocateViews) && LOAD_XR(ResultToString);
#undef LOAD_XR
    }

    void Unload() {
        if (module) FreeLibrary(module);
        module = nullptr;
    }
};
