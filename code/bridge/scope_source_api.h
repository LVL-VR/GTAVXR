#pragma once
// Optional private export. Existing GxApi ABI and normal XR capture unchanged.
#include <stdint.h>
struct GxScopeSourceCapture {
 uint32_t size,version,eye,stage;
 uint64_t frame,generation;
 uint32_t reserved[2];
};
enum { GX_SCOPE_SOURCE_EYE_FINAL_CROP=1u, GX_SCOPE_SOURCE_NATIVE_OPTICAL_VIEW=2u };
// Version1: historical pre-UI eye FINAL. Version2 uses reserved[0] as explicit
// source mode; reserved[1] remains zero. Dedicated optical projection is NOT
// admitted by the old eye cache/consumer. It needs its own qualified producer.
inline bool GxScopeEyeFinalSource(const GxScopeSourceCapture&r){return r.size==sizeof(r)&&!r.reserved[1]&&
 ((r.version==1&&!r.reserved[0])||(r.version==2&&r.reserved[0]==GX_SCOPE_SOURCE_EYE_FINAL_CROP));}
