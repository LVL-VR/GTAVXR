#pragma once
#include "../core/vector_math.h"
// VS2012-compatible anatomical calibration; no game/runtime state.
#include <math.h>
#include <float.h>
namespace gx_hands
{
struct V
{
    double x, y, z;
    V(double a = 0, double b = 0, double c = 0) : x(a), y(b), z(c) {}
};
inline V sub(V a, V b)
{
    return V(a.x - b.x, a.y - b.y, a.z - b.z);
}
inline V add(V a, V b)
{
    return V(a.x + b.x, a.y + b.y, a.z + b.z);
}
inline V mul(V a, double s)
{
    return V(a.x * s, a.y * s, a.z * s);
}
inline double dot(V a, V b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline V cross(V a, V b)
{
    return gx_math::crossProduct(a, b);
}
inline bool unit(V& v)
{
    double n = dot(v, v);
    if (!_finite(n) || n < 1e-10)
        return false;
    v = mul(v, 1 / sqrt(n));
    return true;
}
// Positional calibration is expressed in the physical grip's LOCAL axes and
// applied once before optional palm pitch. It never changes camera or scale.
inline bool translateGrip(double* world, const int* millimeters)
{
    if (!world || !millimeters)
        return false;
    V position(world[12], world[13], world[14]);
    for (unsigned c = 0; c < 3; ++c)
    {
        if (millimeters[c] < -150 || millimeters[c] > 150)
            return false;
        const V axis(world[c * 4], world[c * 4 + 1], world[c * 4 + 2]);
        if (!_finite(dot(axis, axis)) || fabs(dot(axis, axis) - 1) > .01)
            return false;
        position = add(position, mul(axis, millimeters[c] * .001));
    }
    if (!_finite(position.x) || !_finite(position.y) || !_finite(position.z))
        return false;
    world[12] = position.x;
    world[13] = position.y;
    world[14] = position.z;
    return true;
}
// OpenXR grip: -Z little-finger -> thumb, X palm normal (opposite anatomical
// normals for the two hands), Y completes the right-handed frame. Native bind
// finger roots, in hand-bone space, provide a per-model basis rather than guessed Euler offsets.
inline bool grip(V index, V middle, V pinky, double* out)
{
    V y = middle, z = sub(pinky, index);
    if (!unit(y))
        return false;
    z = sub(z, mul(y, dot(z, y)));
    if (!unit(z))
        return false;
    V x = cross(y, z);
    if (!unit(x))
        return false;
    y = cross(z, x);
    V origin = mul(middle, .5); // palm center must NOT move when orientation is corrected.
    // Runtime8 confirms forward fingers but upside-down palms. Keep the accepted
    // finger axis (-Y), roll pi about THAT axis (flip X/Z of build14), not another
    // forward reversal. Both corrections compose to Z180 of the anatomical frame.
    x = mul(x, -1);
    y = mul(y, -1);
    for (int k = 0; k < 16; ++k)
        out[k] = 0;
    out[0] = x.x;
    out[1] = x.y;
    out[2] = x.z;
    out[4] = y.x;
    out[5] = y.y;
    out[6] = y.z;
    out[8] = z.x;
    out[9] = z.y;
    out[10] = z.z;
    out[12] = origin.x;
    out[13] = origin.y;
    out[14] = origin.z;
    out[15] = 1;
    return _finite(origin.x) && _finite(origin.y) && _finite(origin.z);
}
}
