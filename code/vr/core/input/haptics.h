#pragma once
// The native Xbox motor envelope is input, not a new game-event rumble system.
#include "../core/input.h"
#include <math.h>
#include <float.h>
#pragma pack(push, 8)
struct GxRumble
{
    uint32_t size, version;
    uint64_t sequence;
    float heavy, light;
    union
    {
        uint32_t reserved[2];
        float spatial[2];
    }; // v1 reserved-zero; v2 direct L/R copies
};
#pragma pack(pop)
static_assert(sizeof(GxRumble) == 32, "rumble extension POD ABI");
typedef int(GX_CALL* GxSetRumble)(void*, const GxRumble*);
namespace gx_haptic
{
inline float bound(float x)
{
    return _finite(x) ? (x < 0 ? 0 : (x > 1 ? 1 : x)) : 0;
}
struct Motors
{
    float left, right;
    Motors() : left(0), right(0) {}
};
inline bool valid(const GxRumble& v)
{
    if (v.size != sizeof(v))
        return false;
    if (v.version == 1)
        return !v.reserved[0] && !v.reserved[1]; // exact accepted v1 ABI behavior
    return v.version == 2 && _finite(v.heavy) && _finite(v.light) && v.heavy >= 0 && v.heavy <= 1 &&
           v.light >= 0 && v.light <= 1 && _finite(v.spatial[0]) && _finite(v.spatial[1]) &&
           v.spatial[0] >= 0 && v.spatial[0] <= 1 && v.spatial[1] >= 0 && v.spatial[1] <= 1;
}
inline float spatial(const GxRumble& v, unsigned hand)
{
    return v.version == 2 && hand < 2 ? bound(v.spatial[hand]) : 0;
}
inline void enhance(GxRumble& out, bool eligible, bool driving, int percent, bool floorEnabled)
{
    if (!eligible)
    {
        out.heavy = out.light = 0;
        if (out.version == 2)
            out.spatial[0] = out.spatial[1] = 0;
        return;
    }
    const float gain = percent >= 100 && percent <= 200 ? percent * .01f : 1.5f;
    out.heavy = bound(out.heavy * gain);
    out.light = bound(out.light * gain);
    if (out.version == 2)
        for (unsigned h = 0; h < 2; ++h)
            out.spatial[h] = bound(out.spatial[h] * gain);
    if (driving && floorEnabled)
    {
        if (out.heavy < .025f)
            out.heavy = .025f;
        if (out.light < .04f)
            out.light = .04f;
    }
}
inline Motors touch(float heavy, float light)
{
    Motors out;
    heavy = bound(heavy);
    light = bound(light);
    // Xbox has a heavy/low-frequency and a light/high-frequency motor, not two
    // independent spatial hit channels. Both Quest hands retain both components.
    out.left = bound(heavy + .35f * light);
    out.right = bound(light + .35f * heavy);
    return out;
}
}
