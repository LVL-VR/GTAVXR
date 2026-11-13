#pragma once
#include "bridge.h"
// UI source is the native isolated/RESOLVED minimap, never a world backbuffer.
#pragma pack(push,8)
struct GxWristCapture {uint32_t size,version;uint64_t frame,generation;uint32_t x,y,width,height;};
#pragma pack(pop)
static_assert(sizeof(GxWristCapture)==40,"wrist capture ABI");
typedef int (GX_CALL *GxCaptureWrist)(void*,const GxWristCapture*,void*);
