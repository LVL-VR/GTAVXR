#pragma once
// build133 bridge: one-shot per-view size query against an already loaded loader table (mockable).
// Temporary instance only: no session, no swapchain, no device. Destroyed before returning.
#include "eye_size_policy.h"
#include <algorithm>
#include <string.h>
namespace native_xr {
inline uint32_t query_eye_size(OpenXrApi&api,const GxEyeSizeRequest&request,GxEyeSizeResult&out){
    out.mode=request.mode;out.scale_percent=gx_eye_size::clampScale(request.scale_percent);
    XrInstance instance=XR_NULL_HANDLE;const char*extension=XR_KHR_D3D11_ENABLE_EXTENSION_NAME;
    XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
    strcpy_s(ci.applicationInfo.applicationName,"GTAVXR Native Size Query");strcpy_s(ci.applicationInfo.engineName,"GTAVXR");
    ci.applicationInfo.apiVersion=XR_CURRENT_API_VERSION;ci.enabledExtensionCount=1;ci.enabledExtensionNames=&extension;
    if(!api.CreateInstance||XR_FAILED(api.CreateInstance(&ci,&instance))||instance==XR_NULL_HANDLE)return out.status=GX_EYE_RUNTIME;
    uint32_t status=GX_EYE_RUNTIME;
    if(api.LoadForInstance(instance)){
        XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
        if(XR_SUCCEEDED(api.GetInstanceProperties(instance,&properties)))strncpy_s(out.runtime,properties.runtimeName,sizeof(out.runtime)-1);
        XrSystemGetInfo si{XR_TYPE_SYSTEM_GET_INFO};si.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;XrSystemId system=XR_NULL_SYSTEM_ID;
        uint32_t n=0;XrViewConfigurationView views[2]={{XR_TYPE_VIEW_CONFIGURATION_VIEW},{XR_TYPE_VIEW_CONFIGURATION_VIEW}};
        if(XR_SUCCEEDED(api.GetSystem(instance,&si,&system))&&
           XR_SUCCEEDED(api.EnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&n,nullptr))&&n==2&&
           XR_SUCCEEDED(api.EnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,2,&n,views))&&n==2){
            // Both eyes share ONE scene size: the larger recommendation, within the smaller maximum.
            out.recommended_width=(std::max)(views[0].recommendedImageRectWidth,views[1].recommendedImageRectWidth);
            out.recommended_height=(std::max)(views[0].recommendedImageRectHeight,views[1].recommendedImageRectHeight);
            out.max_width=(std::min)(views[0].maxImageRectWidth,views[1].maxImageRectWidth);
            out.max_height=(std::min)(views[0].maxImageRectHeight,views[1].maxImageRectHeight);
            status=gx_eye_size::choose(request.mode,out.recommended_width,out.recommended_height,out.max_width,out.max_height,
                request.scale_percent,request.manual_width,request.manual_height,out.width,out.height,out.clamped)?GX_EYE_OK:GX_EYE_POLICY;
        }else status=GX_EYE_VIEWS;
    }
    if(api.DestroyInstance)api.DestroyInstance(instance);
    return out.status=status;
}
}
