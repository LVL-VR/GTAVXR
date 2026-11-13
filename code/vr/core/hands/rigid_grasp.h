#pragma once
#include "../physics/tracked_physics.h"
 
// plus angular motor. No Havok structures, mass edits, teleports or attachment.
namespace gx_rigid_grasp
{
using namespace gx_physics;
inline V rotate(const Frame& r, V v)
{
    return add(mul(r.a, v.x), add(mul(r.b, v.y), mul(r.c, v.z)));
}
inline Frame desired(const Grasp& g, const Frame& hand)
{
    Frame r;
    r.a = add(mul(hand.a, g.localGrip.a.x), add(mul(hand.b, g.localGrip.b.x), mul(hand.c, g.localGrip.c.x)));
    r.b = add(mul(hand.a, g.localGrip.a.y), add(mul(hand.b, g.localGrip.b.y), mul(hand.c, g.localGrip.c.y)));
    r.c = add(mul(hand.a, g.localGrip.a.z), add(mul(hand.b, g.localGrip.b.z), mul(hand.c, g.localGrip.c.z)));
    r.p = sub(hand.p, rotate(r, g.localGrip.p));
    return r;
}
// Shortest axis-angle, including 180 degrees (cross-axis sums alone vanish there).
inline V rotationError(const Frame& from, const Frame& to)
{
    const V a = add(mul(to.a, from.a.x), add(mul(to.b, from.b.x), mul(to.c, from.c.x)));
    const V b = add(mul(to.a, from.a.y), add(mul(to.b, from.b.y), mul(to.c, from.c.y)));
    const V c = add(mul(to.a, from.a.z), add(mul(to.b, from.b.z), mul(to.c, from.c.z)));
    double w, x, y, z, s;
    const double trace = a.x + b.y + c.z;
    if (trace > 0)
    {
        s = sqrt(trace + 1) * 2;
        w = s * .25;
        x = (b.z - c.y) / s;
        y = (c.x - a.z) / s;
        z = (a.y - b.x) / s;
    }
    else if (a.x > b.y && a.x > c.z)
    {
        s = sqrt(1 + a.x - b.y - c.z) * 2;
        w = (b.z - c.y) / s;
        x = s * .25;
        y = (b.x + a.y) / s;
        z = (c.x + a.z) / s;
    }
    else if (b.y > c.z)
    {
        s = sqrt(1 + b.y - a.x - c.z) * 2;
        w = (c.x - a.z) / s;
        x = (b.x + a.y) / s;
        y = s * .25;
        z = (c.y + b.z) / s;
    }
    else
    {
        s = sqrt(1 + c.z - a.x - b.y) * 2;
        w = (a.y - b.x) / s;
        x = (c.x + a.z) / s;
        y = (c.y + b.z) / s;
        z = s * .25;
    }
    if (w < 0)
    {
        w = -w;
        x = -x;
        y = -y;
        z = -z;
    }
    const double n = sqrt(x * x + y * y + z * z);
    return n > 1e-9 ? mul(V(x, y, z), 2 * atan2(n, w) / n) : V();
}
struct Packet
{
    bool valid;
    V force, torque;
    Packet() : valid(false) {}
};
inline Packet drive(const Grasp& g, const Frame& body, const Frame& hand, V center, V velocity, V angular,
                    const V inertia[3], V gravity, double mass, double dt, unsigned holders, V reference,
                    const gx_tracked_physics::DriveLimits& limits)
{
    Packet out;
    if (!g.held || !g.token.valid() || !g.epoch || !body.rigid() || !hand.rigid() || !finite(center) ||
        !finite(velocity) || !finite(angular) || !inertia || !finite(gravity) || !finite(reference) ||
        norm(reference) > 20 || !finite(mass) || mass <= 0 || mass > 20 || !finite(dt) || dt <= 0 ||
        dt > .05 || holders < 1 || holders > 2 || !limits.valid())
        return out;
    for (unsigned i = 0; i < 3; ++i)
        if (!finite(inertia[i]))
            return out;
    // Actual world inertia must be positive definite and symmetric.
    if (inertia[0].x <= 0 || inertia[1].y <= 0 || inertia[2].z <= 0 ||
        fabs(inertia[0].y - inertia[1].x) > 1e-4 || fabs(inertia[0].z - inertia[2].x) > 1e-4 ||
        fabs(inertia[1].z - inertia[2].y) > 1e-4 ||
        inertia[0].x * inertia[1].y - inertia[0].y * inertia[1].x <= 0 ||
        dot(inertia[0], gx_hands::cross(inertia[1], inertia[2])) <= 0)
        return out;
    const Frame target = desired(g, hand);
    if (!target.rigid())
        return out;
    const double omega = limits.responseRate, den = 1 + 2 * omega * dt + omega * omega * dt * dt;
    const double k = omega * omega / den, d = 2 * omega / den;
    const V error = limited(sub(target.world(body.local(center)), center), limits.error);
    const V accel = sub(sub(mul(error, k), mul(sub(velocity, limited(reference, 6)), d)), gravity);
    const double budget =
        (mass * limits.acceleration < limits.force ? mass * limits.acceleration : limits.force) / holders;
    out.force = limited(mul(accel, mass / holders), budget);
    const V alpha = sub(mul(rotationError(body, target), k), mul(angular, d));
    out.torque =
        limited(mul(add(mul(inertia[0], alpha.x), add(mul(inertia[1], alpha.y), mul(inertia[2], alpha.z))),
                    1.0 / holders),
                limits.torque / holders);
    out.valid = finite(out.force) && finite(out.torque);
    return out;
}
}
