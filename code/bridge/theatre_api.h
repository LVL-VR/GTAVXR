#pragma once
// Optional v1 export GtavxrComposeTheatre1; does not extend or change GxApi. No STL crosses this ABI.
// The game asks for one stereo pair filmed by its own cutscene camera. The bridge answers with the two
// eye views and, for that frame only, presents the pair on a screen fixed in the tracking space.
#include <stdint.h>
#pragma pack(push, 8)
struct GxTheatre {
    uint32_t size, version;      // version 1
    uint32_t placement;          // a new value places the screen again in front of the head (game recenter)
    uint32_t reserved;           // zero in v1
    double vertical_fov_degrees; // authored vertical field of view of the filming camera
    double aspect;               // authored width / height of the frame
    double near_z, far_z;        // clip planes of this update
};
#pragma pack(pop)
// Return values of GtavxrComposeTheatre1: armed for this frame; refused without a fault (the caller
// presents this frame with its ordinary camera); any other value is the usual bridge fault.
enum { GX_THEATRE_ARMED = 0, GX_THEATRE_REFUSED = 2 };
