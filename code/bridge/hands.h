#pragma once
#include "bridge.h"
#pragma pack(push, 8)
// Independent extension: old GxApi/GxInput layouts remain byte-for-byte unchanged.
struct GxHandTargets {
    uint32_t size, version, valid_mask, reserved;
    uint64_t frame, generation;
    int64_t display_time;
    GxPose local[2];
    double world[2][16];
};
#pragma pack(pop)
static_assert(sizeof(GxHandTargets)==408,"hand targets POD ABI");
typedef int (GX_CALL *GxLocateHands)(void*,const GxFrame*,const double*,GxHandTargets*);
