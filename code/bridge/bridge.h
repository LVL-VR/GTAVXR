#pragma once
// GTAVXR-owned ABI. No STL, exceptions or allocated memory cross the VS2012/DLL boundary.
#include <stdint.h>
#ifdef _WIN32
#define GX_CALL __cdecl
#else
#define GX_CALL
#endif
#pragma pack(push, 8)
struct GxPose { double x,y,z,qx,qy,qz,qw; };
struct GxFov { double left,right,up,down; };
struct GxFrame {
    uint32_t size, renderable;
    uint64_t id, generation;
    int64_t display_time;
    GxPose reference, eyes[2];
    GxFov fov[2];
};
struct GxViews {
    uint32_t size, reserved;
    double center[16], camera[2][16], projection[2][16];
    double center_fov_degrees, center_aspect, center_near, center_far;
};
struct GxConfig {
    uint32_t size, width, height, reserved;
    void* d3d_device;
    const wchar_t* loader_path;
    const wchar_t* log_path;
};
struct GxApi {
    uint32_t size, version;
    int (GX_CALL *create)(const GxConfig*, void**);
    int (GX_CALL *begin)(void*, uint64_t generation, GxFrame*);
    int (GX_CALL *compose)(const GxFrame*, const double* gameplay_camera,
                          double near_z, double far_z, GxViews*);
    int (GX_CALL *capture)(void*, uint64_t frame, uint64_t generation,
                          uint32_t eye, void* d3d_texture);
    int (GX_CALL *finish)(void*, uint64_t frame, uint64_t generation);
    int (GX_CALL *cancel)(void*);
    int (GX_CALL *destroy)(void*);
    const char* (GX_CALL *error)(void*);
};
#pragma pack(pop)
typedef int (GX_CALL *GxGetApi)(uint32_t, uint32_t, GxApi*);
// Return0 success. compose ONLY may return2 for a recoverable camera frame:
// caller must cancel/omit that pair before another frame; other nonzero is fatal.
