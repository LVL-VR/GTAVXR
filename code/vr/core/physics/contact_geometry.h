#pragma once
#include <stdint.h>
// Owned CPU-only contact geometry. No buffer writes, readback or graphics state.
#include "../body/avatar_pose.h"
#include "../hands/hand_basis.h"
#include <vector>
namespace gx_contact
{
typedef gx_hands::V V;
struct Triangle
{
    V a, b, c;
    Triangle(V x, V y, V z) : a(x), b(y), c(z) {}
};
inline double length2(V v)
{
    return gx_hands::dot(v, v);
}
// Supplied by the integration host; excluded data/code stay outside the core.
bool closest(const Triangle& t, V p, V& out);
class Mesh
{
    std::vector<Triangle> triangles_;

  public:
    void clear()
    {
        triangles_.clear();
    }
    size_t size() const
    {
        return triangles_.size();
    }
    bool add(V a, V b, V c, size_t limit = 32768)
    {
        if (triangles_.size() >= limit || !gx_avatar::finite(a) || !gx_avatar::finite(b) ||
            !gx_avatar::finite(c))
            return false;
        if (length2(gx_hands::cross(gx_hands::sub(b, a), gx_hands::sub(c, a))) < 1e-20)
            return true;
        triangles_.push_back(Triangle(a, b, c));
        return true;
    }
    bool nearest(V p, V& point, double& distance) const
    {
        if (!gx_avatar::finite(p) || triangles_.empty())
            return false;
        double best = 1e100;
        V selected;
        for (size_t n = 0; n < triangles_.size(); ++n)
        {
            V q;
            if (!closest(triangles_[n], p, q))
                return false;
            const double d = length2(gx_hands::sub(p, q));
            if (d < best)
            {
                best = d;
                selected = q;
            }
        }
        if (!_finite(best))
            return false;
        point = selected;
        distance = sqrt(best);
        return true;
    }
};
// Press-away then move-near is NOT a new grip. Missing input is not a release.
class GripEdges
{
    bool down_[2];

  public:
    GripEdges()
    {
        reset();
    }
    void reset()
    {
        down_[0] = down_[1] = true;
    }
    unsigned update(unsigned active, const float* grip)
    {
        unsigned edges = 0;
        if (!grip)
            return 0;
        for (unsigned h = 0; h < 2; ++h)
            if ((active & (1u << h)) && _finite(grip[h]))
            {
                if (grip[h] < .45f)
                    down_[h] = false;
                else if (grip[h] >= .65f)
                {
                    if (!down_[h])
                        edges |= 1u << h;
                    down_[h] = true;
                }
            }
        return edges;
    }
};
inline bool nearSurface(double distance, double tolerance = .035)
{
    return _finite(distance) && _finite(tolerance) && distance >= 0 && tolerance >= 0 &&
           distance <= tolerance;
}
// Continuous physical acquisition is separate from button-edge telemetry.
// An already-owned hand never transfers from weapon/UI to a cockpit control.
inline bool canAcquire(unsigned hand, unsigned active, const float* grip, bool occupied)
{
    return hand < 2 && grip && (active & (1u << hand)) && _finite(grip[hand]) && grip[hand] >= .65f &&
           !occupied;
}
// Native left/right handlebar IK marker supplies the center and the opposite
// marker supplies the lateral axis. Two disjoint lateral capsules, never one
// central sphere. Cover the grip plus hand-width tolerance along the bar; cap
// extent by marker separation so even narrow bars retain an excluded center.
inline bool handleGrip(V point, V marker, V other)
{
    if (!gx_avatar::finite(point) || !gx_avatar::finite(marker) || !gx_avatar::finite(other))
        return false;
    V axis = gx_hands::sub(marker, other);
    const double span = sqrt(length2(axis));
    if (span < .20 || span > 1.8)
        return false;
    axis = gx_hands::mul(axis, 1 / span);
    const V offset = gx_hands::sub(point, marker);
    double along = gx_hands::dot(offset, axis);
    const double halfLength = span * .22 < .15 ? span * .22 : .15;
    const double radius = span * .10 < .06 ? span * .10 : .06;
    if (along > halfLength)
        along = halfLength;
    if (along < -halfLength)
        along = -halfLength;
    return length2(gx_hands::sub(offset, gx_hands::mul(axis, along))) <= radius * radius;
}
// Retain a support point IN WEAPON SPACE for one press, never recapture every frame.
class SupportContact
{
    uint64_t owner_;
    bool down_, held_;
    V point_;

  public:
    SupportContact() : owner_(0), down_(false), held_(false) {}
    void reset()
    {
        owner_ = 0;
        down_ = held_ = false;
    }
    void observeRelease(float trigger, bool active)
    {
        if (active && _finite(trigger) && trigger < .2f)
            down_ = held_ = false;
    }
    bool update(uint64_t owner, float trigger, bool valid, bool onSurface, V localPoint)
    {
        if (!owner || !_finite(trigger))
        {
            reset();
            return false;
        }
        if (owner != owner_)
        {
            const bool changed = owner_ != 0;
            owner_ = owner;
            held_ = false;
            down_ = changed && trigger >= .2f;
        }
        if (trigger < .2f)
        {
            down_ = held_ = false;
            return false;
        }
        if (trigger >= .55f && !down_)
        {
            down_ = true;
            held_ = valid && onSurface && gx_avatar::finite(localPoint);
            if (held_)
                point_ = localPoint;
        }
        return held_ && valid;
    }
    V point() const
    {
        return point_;
    }
    bool held() const
    {
        return held_;
    }
};
inline bool rigidSupport(const double* gun, V pivot, V localPoint, V support, double* out, double& residual)
{
    if (!gun || !out || !gx_avatar::finite(pivot) || !gx_avatar::finite(localPoint) ||
        !gx_avatar::finite(support))
        return false;
    V columns[4];
    for (unsigned c = 0; c < 4; ++c)
    {
        columns[c] = V(gun[c * 4], gun[c * 4 + 1], gun[c * 4 + 2]);
        if (!gx_avatar::finite(columns[c]))
            return false;
    }
    for (unsigned c = 0; c < 3; ++c)
        if (fabs(length2(columns[c]) - 1) > .01)
            return false;
    if (fabs(gx_hands::dot(columns[0], columns[1])) > .01 ||
        fabs(gx_hands::dot(columns[0], columns[2])) > .01 ||
        fabs(gx_hands::dot(columns[1], columns[2])) > .01 ||
        gx_hands::dot(gx_hands::cross(columns[0], columns[1]), columns[2]) < .99)
        return false;
    const V point =
        gx_hands::add(columns[3], gx_hands::add(gx_hands::mul(columns[0], localPoint.x),
                                                gx_hands::add(gx_hands::mul(columns[1], localPoint.y),
                                                              gx_hands::mul(columns[2], localPoint.z))));
    const V from = gx_hands::sub(point, pivot), to = gx_hands::sub(support, pivot);
    const double a = sqrt(length2(from)), b = sqrt(length2(to));
    if (a < .10 || b < .001)
        return false;
    // Trigger support is directional even when physical hands are closer/further
    // than the immutable model span. Preserve that radial residual; NEVER use it
    // to detach, scale the gun or move a tracker. Exact two-point contact exists
    // only at equal spans, but the fixed foregrip always points toward the hand.
    residual = fabs(a - b);
    for (unsigned c = 0; c < 4; ++c)
    {
        V value = c == 3 ? gx_hands::sub(columns[c], pivot) : columns[c], rotated;
        if (!gx_avatar::swing(from, to, value, rotated))
            return false;
        if (c == 3)
            rotated = gx_hands::add(rotated, pivot);
        out[c * 4] = rotated.x;
        out[c * 4 + 1] = rotated.y;
        out[c * 4 + 2] = rotated.z;
        out[c * 4 + 3] = c == 3 ? 1 : 0;
    }
    return true;
}
}
