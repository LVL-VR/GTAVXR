#pragma once
#include "bridge.h"
// Separate POD extension; no change to the existing frame/input ABI.
#pragma pack(push, 8)
struct GxOverlay
{
    // version1=list, version2=radial weapon selection. Same fixed POD layout.
    uint32_t size, version, visible, selected, row_count, revision;
    char title[64], rows[12][80], footer[128];
};
#pragma pack(pop)
static_assert(sizeof(GxOverlay) == 1176, "overlay POD ABI");
typedef int(GX_CALL* GxSetOverlay)(void*, const GxOverlay*);
