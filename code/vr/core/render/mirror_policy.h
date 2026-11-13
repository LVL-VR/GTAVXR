#pragma once
#include <float.h>
namespace gx_mirror
{
// Native mirror producer uses symmetric camera coordinates. Eye NDC contains
// -projection[8/9]; undo that shear at sampling, not by moving eye geometry.
inline bool bounds(const float* nativeBounds, float shearX, float shearY, float* out)
{
    if (!nativeBounds || !out || !_finite(shearX) || !_finite(shearY))
        return false;
    for (unsigned i = 0; i < 4; ++i)
        if (!_finite(nativeBounds[i]))
            return false;
    out[0] = nativeBounds[0] + nativeBounds[2] * shearX;
    out[1] = nativeBounds[1] + nativeBounds[3] * shearY;
    out[2] = nativeBounds[2];
    out[3] = nativeBounds[3];
    return _finite(out[0]) && _finite(out[1]);
}
// Render-thread-only state: each eye barrier invalidates the previous eye;
// native mirror draw-list command supplies new bounds before world consumers.
struct State
{
    float value[4];
    int eye;
    bool valid;
    State() : eye(-1), valid(false) {}
    void begin(int e)
    {
        eye = e;
        valid = false;
    }
    void set(float x, float y, float z, float w)
    {
        valid = eye == 0 || eye == 1;
        value[0] = x;
        value[1] = y;
        value[2] = z;
        value[3] = w;
        for (unsigned i = 0; i < 4; ++i)
            if (!_finite(value[i]))
                valid = false;
    }
    bool read(int e, float* out) const
    {
        if (!valid || e != eye || !out)
            return false;
        for (unsigned i = 0; i < 4; ++i)
            out[i] = value[i];
        return true;
    }
};
}
