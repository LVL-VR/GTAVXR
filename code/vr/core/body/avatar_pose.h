#pragma once
// Own visual-pose mathematics. VS2012-compatible, shared with offline controls.
// No simulation state, game callbacks, allocations or runtime dependencies.
#include "../hands/hand_basis.h"
namespace gx_avatar
{
using gx_hands::V;
using gx_hands::add;
using gx_hands::sub;
using gx_hands::mul;
using gx_hands::dot;
using gx_hands::cross;
using gx_hands::unit;
inline bool finite(V v)
{
    return _finite(v.x) && _finite(v.y) && _finite(v.z);
}
inline double length(V v)
{
    return sqrt(dot(v, v));
}
struct Arm
{
    V elbow;
    double upper, lower;
    bool stretched;
};
// Endpoint is never clipped to the native actor's arm length. If physical reach
// exceeds it, distribute extension across BOTH segments, not a wrist-only gap.
// Pole is in body object space, independent of the native combat stance.
inline bool arm(V shoulder, V wrist, V pole, double upper, double lower, Arm& out)
{
    if (!finite(shoulder) || !finite(wrist) || !finite(pole) || !_finite(upper) || !_finite(lower) ||
        upper < .01 || lower < .01)
        return false;
    V axis = sub(wrist, shoulder);
    const double d = length(axis);
    if (d < 1e-6)
        return false;
    axis = mul(axis, 1 / d);
    V bend = sub(pole, mul(axis, dot(pole, axis)));
    if (!unit(bend))
    {
        V fallback = fabs(axis.z) < .9 ? V(0, 0, -1) : V(1, 0, 0);
        bend = sub(fallback, mul(axis, dot(fallback, axis)));
        if (!unit(bend))
            return false;
    }
    const double original = upper + lower;
    // Avoid the unstable perfectly straight elbow at full extension.
    if (d > original * .985)
    {
        const double scale = d / (original * .985);
        upper *= scale;
        lower *= scale;
    }
    // A wrist folded very close to the shoulder needs equal visual segments.
    // This is visual retargeting, not a promise of anatomical/physics constraints.
    if (d < fabs(upper - lower) + 1e-5)
        upper = lower = (upper + lower) * .5;
    const double along = (d * d + upper * upper - lower * lower) / (2 * d);
    double h2 = upper * upper - along * along;
    if (h2 < 0 && h2 > -1e-9)
        h2 = 0;
    if (h2 < 0 || !_finite(h2))
        return false;
    out.elbow = add(shoulder, add(mul(axis, along), mul(bend, sqrt(h2))));
    out.upper = upper;
    out.lower = lower;
    out.stretched = upper + lower > original + 1e-6;
    return finite(out.elbow);
}
// Shortest proper rotation from one bone direction to another. Native neutral
// twist is transported instead of inventing left/right bone Euler conventions.
inline bool swing(V from, V to, V value, V& out)
{
    if (!finite(value) || !unit(from) || !unit(to))
        return false;
    double c = dot(from, to);
    if (c > 1)
        c = 1;
    if (c < -1)
        c = -1;
    if (c > 1 - 1e-12)
    {
        out = value;
        return true;
    }
    V axis = cross(from, to);
    if (c < -1 + 1e-10)
    {
        axis = cross(from, fabs(from.z) < .9 ? V(0, 0, 1) : V(1, 0, 0));
        if (!unit(axis))
            return false;
        out = sub(mul(axis, 2 * dot(axis, value)), value);
        return true;
    }
    // Rodrigues with non-unit cross product; denominator safe outside antipodes.
    out = add(add(value, cross(axis, value)), mul(cross(axis, cross(axis, value)), 1 / (1 + c)));
    return finite(out);
}
inline V elbowPole(unsigned hand)
{
    return V(hand ? .75 : -.75, .2, -.65);
}
inline bool visualPass(bool fps, bool completed)
{
    return fps && completed;
}
// Bind-pose anatomical axes need not equal the ped's gameplay axes. In the
// runtime9 actor, right-minus-left shoulder points toward -X in bind space.
// Build a yaw-only proper rotation into ped +X right/+Y forward; don't rotate HMD.
inline bool bindAxes(V leftShoulder, V rightShoulder, V& right, V& forward)
{
    right = sub(rightShoulder, leftShoulder);
    right.z = 0;
    if (!finite(right) || !unit(right))
        return false;
    forward = cross(V(0, 0, 1), right);
    return finite(forward);
}
inline V bindToPed(V v, V right, V forward)
{
    return V(dot(v, right), dot(v, forward), v.z);
}
inline V standingEye(V base, V bindEye)
{
    return V(base.x, base.y, base.z + bindEye.z);
}
inline V rootAtEyes(V eyes, V bindEye, V right, V forward, V up)
{
    return sub(eyes, add(add(mul(right, bindEye.x), mul(forward, bindEye.y)), mul(up, bindEye.z)));
}
}
