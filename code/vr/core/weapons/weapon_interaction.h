#pragma once
#include "../physics/physical_hand_motion.h"
#include <math.h>
namespace gx_physical_hand_motion
{
using gx_physics::V;
using gx_physics::Frame;
inline double segmentDistance(V p, V a, V b)
{
    const V ab = gx_physics::sub(b, a);
    const double n = gx_physics::dot(ab, ab);
    double t = n > 1e-12 ? gx_physics::dot(gx_physics::sub(p, a), ab) / n : 0;
    if (t < 0)
        t = 0;
    if (t > 1)
        t = 1;
    return gx_physics::norm(gx_physics::sub(p, gx_physics::add(a, gx_physics::mul(ab, t))));
}
// Reload-part constraints retain a small finite controller reach.
inline bool handReachable(const Frame& tracked, const Frame& fixed)
{
    if (!tracked.rigid() || !fixed.rigid() || gx_physics::norm(gx_physics::sub(tracked.p, fixed.p)) > .30)
        return false;
    const double cosine = (gx_physics::dot(tracked.a, fixed.a) + gx_physics::dot(tracked.b, fixed.b) +
                           gx_physics::dot(tracked.c, fixed.c) - 1) *
                          .5;
    return cosine >= .6427876096865394;
}
// A tracked controller rate, with reload quarantine and identity/tracking reseeding.
// The rendered weapon contributes only the coordinate basis, never a motion delta.
class Motion
{
    Frame previous_;
    double at_, quietUntil_;
    uint64_t weapon_;
    unsigned hand_;
    bool valid_, reloading_;
    gx_hand_motion::Stamp stamp_;
    gx_hand_motion::Hand physical_;
    bool mapped_;

  public:
    Motion() : at_(0), quietUntil_(0), weapon_(0), hand_(2), valid_(false), reloading_(false), mapped_(false)
    {
    }
    static double QUIET()
    {
        return .35;
    }
    // pose is RAW LOCAL controller orientation. map only rotates the resulting physical rate.
    V update(uint64_t weapon, unsigned hand, bool tracked, bool reloading, double now, const Frame& pose,
             const Frame& basis, const Frame& map = Frame())
    {
        V out;
        const double dt = now - at_;
        const bool same = valid_ && weapon == weapon_ && hand == hand_;
        if (reloading || reloading_)
            quietUntil_ = now + QUIET();
        if (tracked && pose.rigid() && basis.rigid() && same && !reloading && now >= quietUntil_ &&
            dt > .002 && dt < .1)
        {
            V skew = gx_physics::add(
                gx_hands::cross(previous_.a, pose.a),
                gx_physics::add(gx_hands::cross(previous_.b, pose.b), gx_hands::cross(previous_.c, pose.c)));
            skew = gx_physics::mul(skew, .5);
            double cosine = (gx_physics::dot(previous_.a, pose.a) + gx_physics::dot(previous_.b, pose.b) +
                             gx_physics::dot(previous_.c, pose.c) - 1) *
                            .5;
            if (cosine > 1)
                cosine = 1;
            if (cosine < -1)
                cosine = -1;
            const double sine = gx_physics::norm(skew);
            const V local = gx_physics::mul(skew, sine > 1e-8 ? atan2(sine, cosine) / (sine * dt) : 1 / dt);
            const V world = gx_hand_motion::rotate(map, local);
            out = V(gx_physics::dot(world, basis.a), gx_physics::dot(world, basis.b),
                    gx_physics::dot(world, basis.c));
        }
        previous_ = pose;
        at_ = now;
        weapon_ = weapon;
        hand_ = hand;
        valid_ = tracked && pose.rigid();
        reloading_ = reloading;
        return out;
    }
    V updatePhysical(uint64_t weapon, unsigned hand, bool tracked, bool reloading,
                     const gx_hand_motion::Publication& p, const Frame& basis)
    {
        const bool usable =
            tracked && hand < 2 && p.stamp.usable() && (p.mask & (1u << hand)) && p.hand[hand].usable();
        if (!usable)
        {
            valid_ = mapped_ = false;
            return V();
        }
        const gx_hand_motion::Hand& h = p.hand[hand];
        if (!mapped_ || !gx_hand_motion::consecutive(stamp_, p.stamp) || !h.sameCalibration(physical_))
            valid_ = false;
        const V out = update(weapon, hand, true, reloading, p.stamp.time, h.raw, basis, h.map);
        stamp_ = p.stamp;
        physical_ = h;
        mapped_ = true;
        return out;
    }
};
}
