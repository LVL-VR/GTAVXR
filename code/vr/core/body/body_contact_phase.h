#pragma once
// Native animated-body contact phase policy. Never modifies a tracked pose or
// integrates an object. Physics consumes the last completed, unified VRIK pose.
#include "../physics/physics_contact.h"
namespace gx_body_phase
{
struct Stamp
{
    uintptr_t actor, instance, bound, skeleton, data;
    unsigned identity, level, generation, lod, components;
    uint64_t pose, tick;
    double time;
    gx_physics::V root; // tick: ms freshness only; time: QPC seconds for dt (build147 A)
    Stamp()
        : actor(0), instance(0), bound(0), skeleton(0), data(0), identity(0), level(65535), generation(0),
          lod(0), components(0), pose(0), tick(0), time(0)
    {
    }
    bool valid() const
    {
        return actor && instance && bound && skeleton && data && identity && level < 65535 &&
               generation <= 65535 && components && components <= 32 && pose && tick &&
               gx_physics::finite(root);
    }
    bool same(const Stamp& s) const
    {
        return valid() && s.valid() && actor == s.actor && instance == s.instance && bound == s.bound &&
               skeleton == s.skeleton && data == s.data && identity == s.identity && level == s.level &&
               generation == s.generation && lod == s.lod && components == s.components;
    }
};
inline bool fresh(const Stamp& s, uint64_t now)
{
    return s.valid() && now >= s.tick && now - s.tick <= 100;
}
inline double inverse(const Stamp& previous, const Stamp& current, double maximumShift, double maximumTurn)
{
    if (!previous.same(current) || current.pose <= previous.pose || !(current.time > previous.time) ||
        current.time - previous.time > .1 ||
        gx_physics::norm(gx_physics::sub(current.root, previous.root)) > .35 ||
        !gx_physics::finite(maximumShift) || !gx_physics::finite(maximumTurn))
        return 0;
    const double dt = current.time - previous.time;
    // Teleports/recenter/skeleton replacement seed last=current. Never turn a
    // discontinuity into an impulse or divide tracker speed by slow game time.
    return dt >= .001 && maximumShift <= 6 * dt && maximumTurn <= 20 * dt ? 1 / dt : 0;
}
// Solid-hand separation is virtual ONLY. A failed query/initial overlap or an
// anatomical escape resumes raw tracking; no native-animation snap or HMD pull.
inline bool useSolved(const gx_physics::Solved& s, gx_physics::V raw, double armLength)
{
    return s.valid && gx_physics::finite(s.position) && gx_physics::finite(raw) &&
           gx_physics::finite(armLength) && armLength >= .02 && armLength <= 3 &&
           gx_physics::norm(gx_physics::sub(s.position, raw)) <= armLength + .10;
}
}
