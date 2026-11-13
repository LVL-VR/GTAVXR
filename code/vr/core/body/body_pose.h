#pragma once
// GTAVXR-owned body retarget contract; no engine, camera, input or physics writes.
// Shared by the native adapter and headless controls. VS2012 compatible.
#include "avatar_pose.h"
namespace gx_body
{
using gx_avatar::V;
using gx_avatar::finite;
using gx_avatar::length;
using gx_hands::add;
using gx_hands::sub;
using gx_hands::mul;
using gx_hands::dot;
enum Joint
{
    Pelvis,
    SpineRoot,
    Spine0,
    Spine1,
    Spine2,
    Spine3,
    LeftClavicle,
    RightClavicle,
    JointCount
};
inline bool writable(int bone, int count, int terminal, int parent)
{
    return bone >= 0 && bone < count && terminal > bone && terminal <= count &&
           ((bone == 0 && parent == -1) || (parent >= 0 && parent < bone));
}
// Pelvis extension must NEVER become a prerequisite for the accepted torso fix.
// Explicit object-space roots counter any animated/virtual parent stance; resetting
// descendant locals alone is not equivalent. Missing optional joints are independent.
template <class Skeleton> unsigned neutral(Skeleton& skel)
{
    unsigned result = 0;
    if (skel.neutralObject(Pelvis))
        result |= 1u << Pelvis;
    if (skel.neutralObject(SpineRoot))
    {
        result |= 1u << SpineRoot;
        for (unsigned j = Spine0; j < JointCount; ++j)
            if (skel.resetLocal((Joint)j))
                result |= 1u << j;
    }
    return result;
}
struct Epoch
{
    const void* owner;
    const void* layout;
    unsigned long long frame;
    Epoch() : owner(0), layout(0), frame(0) {}
    void clear()
    {
        owner = layout = 0;
        frame = 0;
    }
    void capture(const void* p, const void* d, unsigned long long f)
    {
        owner = p;
        layout = d;
        frame = f;
    }
    bool matches(const void* p, const void* d, unsigned long long f) const
    {
        return f && owner && layout && owner == p && layout == d && frame == f;
    }
};
// Transport the native step relative to animated hip axes into the neutral VR
// heading. Vertical foot motion and native gait timing remain unchanged. Anchor
// stays in native world space, so HMD lean/crouch does not drag feet with the head.
inline V gaitVector(V value, V sourceRight, V sourceForward, V right, V forward)
{
    return add(add(mul(right, dot(value, sourceRight)), mul(forward, dot(value, sourceForward))),
               V(0, 0, value.z));
}
inline V gaitPoint(V point, V anchor, V sourceRight, V sourceForward, V right, V forward)
{
    return add(anchor, gaitVector(sub(point, anchor), sourceRight, sourceForward, right, forward));
}
struct Leg
{
    V knee, foot;
    double residual;
};
// Unlike tracked arms, legs must not inherit arm()'s unlimited segment extension.
// Retain both bind lengths. Explicit residual reports an unreachable animated
// endpoint instead of stretching bones or moving the HMD to make it fit.
inline bool leg(V hip, V foot, V pole, double upper, double lower, Leg& out)
{
    if (!finite(hip) || !finite(foot) || !finite(pole) || !_finite(upper) || !_finite(lower) || upper < .01 ||
        lower < .01)
        return false;
    V axis = sub(foot, hip);
    double d = length(axis);
    if (d < 1e-8 || !_finite(d))
        return false;
    axis = mul(axis, 1 / d);
    const double minimum = fabs(upper - lower) + 1e-6, maximum = upper + lower - 1e-6;
    double reach = d;
    if (reach < minimum)
        reach = minimum;
    if (reach > maximum)
        reach = maximum;
    out.foot = add(hip, mul(axis, reach));
    out.residual = length(sub(out.foot, foot));
    V bend = sub(pole, mul(axis, dot(pole, axis)));
    if (!gx_hands::unit(bend))
    {
        bend = sub(V(0, 1, 0), mul(axis, axis.y));
        if (!gx_hands::unit(bend))
        {
            bend = sub(V(1, 0, 0), mul(axis, axis.x));
            if (!gx_hands::unit(bend))
                return false;
        }
    }
    const double along = (reach * reach + upper * upper - lower * lower) / (2 * reach);
    double h2 = upper * upper - along * along;
    if (h2 < 0 && h2 > -1e-9)
        h2 = 0;
    if (h2 < 0 || !_finite(h2))
        return false;
    out.knee = add(hip, add(mul(axis, along), mul(bend, sqrt(h2))));
    return finite(out.knee) && finite(out.foot);
}
}
