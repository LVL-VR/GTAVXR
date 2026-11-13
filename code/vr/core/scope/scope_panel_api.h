#pragma once
#include <stdint.h>
// build143 F5 bridge contract (GtavxrSetScopePanel1): the frame's ONE native image (window backbuffer, captured as eye 0;
// eye 1 = counted no-op) is shown as ONE opaque head-locked quad to BOTH eyes or ONE eye; nothing else is submitted, so the
// other eye is black. Call after begin and before any capture of that frame. Additive export; old GxApi unchanged.
#pragma pack(push, 8)
struct GxScopePanel
{
    uint32_t size, version;
    uint32_t eyes;
    uint32_t reserved;
    double width_degrees, distance_m;
};
#pragma pack(pop)
enum
{
    GX_SCOPE_PANEL_LEFT = 1,
    GX_SCOPE_PANEL_RIGHT = 2,
    GX_SCOPE_PANEL_BOTH = 3
};
