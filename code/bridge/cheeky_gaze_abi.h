#pragma once
// Layout-compatible declaration of CheekyFoveatedDLSS's process-local gaze snapshot (shared/cheeky_gaze_abi.h in the
// Cheeky fork, ABI version 4). Cheeky's runtime reads it through CheekyOpenXR_GetGazeSnapshot, exported by any module
// named CheekyOpenXRLayer.dll. GTAVXR builds a shim of that name (cheeky_gaze_shim.cpp); the bridge fills it.
#include <stdint.h>

namespace gx_cheeky_abi {
constexpr uint32_t Version = 4, MaxViews = 2, RuntimeNameSize = 128;
enum : uint32_t {
    LayerActive = 1u << 0, ExtensionEnabled = 1u << 1, SystemSupported = 1u << 2, SessionFocused = 1u << 3,
    ActionActive = 1u << 4, GazeValid = 1u << 5, MappingReady = 1u << 6,
};
enum : uint32_t {
    ViewOrientationValid = 1u << 0, ViewPositionValid = 1u << 1, ViewOrientationTracked = 1u << 2,
    ViewPositionTracked = 1u << 3, ViewResourceValid = 1u << 4, ViewFovValid = 1u << 6, ViewForwardValid = 1u << 7,
};
struct View {
    uint32_t structure_size, view_index, flags, array_index;
    float center_u, center_v;
    int32_t image_rect_x, image_rect_y;
    uint32_t image_rect_width, image_rect_height;
    uint64_t resource_identity, swapchain_identity;
    float next_jump_u, next_jump_v;
    float fov_left, fov_right, fov_up, fov_down;
    float forward_u, forward_v;
};
struct Snapshot {
    uint32_t abi_version, structure_size, status_flags, view_count;
    uint64_t sequence, publication_qpc;
    int64_t predicted_display_time, sample_time;
    uint64_t session_generation, swapchain_generation;
    char runtime_name[RuntimeNameSize];
    View views[MaxViews];
};
static_assert(sizeof(View) == 88, "Cheeky gaze view ABI");
static_assert(sizeof(Snapshot) == 64 + RuntimeNameSize + 2 * sizeof(View), "Cheeky gaze snapshot ABI");
// Exported by the shim for the bridge.
typedef void(__cdecl* PublishFn)(const Snapshot*);
}
