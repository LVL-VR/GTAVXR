#pragma once
// Optional v1 export; does not extend or change GxApi. No STL crosses this ABI.
#include <stdint.h>
#pragma pack(push, 8)
struct GxScopePanel {
    uint32_t size, version;
    uint32_t eyes; // 1 LEFT, 2 RIGHT, 3 BOTH
    uint32_t reserved; // zero in v1
    double width_degrees, distance_m;
};
#pragma pack(pop)

