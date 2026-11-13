#pragma once
// Independent versioned POD extension; the proven graphics ABI remains unchanged.
#include "bridge.h"
enum GxButton
{
    GX_A = 1,
    GX_B = 2,
    GX_X = 4,
    GX_Y = 8,
    GX_LCLICK = 16,
    GX_RCLICK = 32,
    GX_MENU = 64
};
#pragma pack(push, 8)
struct GxInput
{
    uint32_t size, version, active_hands, buttons;
    uint64_t sequence;
    int64_t display_time;
    float stick[2][2], trigger[2], grip[2];
    uint32_t head_valid, reserved;
    GxPose head, reference;
};
#pragma pack(pop)
static_assert(sizeof(GxInput) == 184, "input POD ABI");
typedef int(GX_CALL* GxReadInput)(void*, GxInput*);
typedef int(GX_CALL* GxSetPanel)(void*);
// build165 item 18: the runtime floor (OpenXR STAGE), optional bridge export GtavxrReadFloor1; valid 0 = no floor.
#pragma pack(push, 8)
struct GxFloor
{
    uint32_t size, version, valid, reserved;
    double local_above_floor;
};
#pragma pack(pop)
static_assert(sizeof(GxFloor) == 24, "floor POD ABI");
typedef int(GX_CALL* GxReadFloor)(void*, GxFloor*);
