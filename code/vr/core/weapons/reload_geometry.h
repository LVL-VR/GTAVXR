#pragma once
#include "weapon_interaction.h"
// Shared by the native glue and the CPU walks. All lengths are metres.
namespace gx_reload_geometry
{
using gx_physics::V;
using gx_physics::Frame;
inline double clamp(double x, double lo, double hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}
inline double boxDistance(V p, V lo, V hi)
{
    const V q(clamp(p.x, lo.x, hi.x), clamp(p.y, lo.y, hi.y), clamp(p.z, lo.z, hi.z));
    return gx_physics::norm(gx_physics::sub(p, q));
}
// A measured bone-local box, including the part's translation over its stroke.
inline double zoneDistance(V hand, const Frame& bone, V lo, V hi, double stroke = 0)
{
    if (stroke >= 0)
        lo.x -= stroke;
    else
        hi.x -= stroke;
    return boxDistance(bone.local(hand), lo, hi);
}
inline bool inZone(double distance)
{
    return gx_physics::finite(distance) && distance <= .04;
}
struct Gate
{
    double distance, lateral, axis, front, depth;
    bool at, on;
    Gate() : distance(0), lateral(0), axis(0), front(0), depth(0), at(false), on(false) {}
};
// Magazine travel is measured at receiver vertices bordering the default clip footprint.
// The mouth capture adds 40 mm of hand tolerance to a 30 mm well neighbourhood.
// Release adds the same 40 mm to the capture radius, providing hysteresis.
inline Gate magazine(V root, V axis, V front, const Frame& seat, V u, double stroke)
{
    Gate g;
    if (stroke <= .001)
        return g;
    const V mouth = gx_physics::sub(seat.p, gx_physics::mul(u, stroke));
    const V r = gx_physics::sub(root, mouth);
    const double t = gx_physics::dot(r, u);
    g.distance = gx_physics::norm(r);
    g.lateral = gx_physics::norm(gx_physics::sub(r, gx_physics::mul(u, t)));
    g.depth = t / stroke;
    g.axis = gx_physics::dot(axis, u);
    g.front = gx_physics::dot(front, seat.a);
    g.at = g.distance <= .07 && g.axis >= .7071067811865476;
    g.on = g.lateral <= .11 && t >= -.07;
    return g;
}
// Once captured, only the hand's axial displacement moves the aligned magazine.
// A magazine approaching past the mouth starts at the mouth and still needs a stroke.
class Magnet
{
    bool active_;
    V captureHand_;
    double start_;

  public:
    Magnet() : active_(false), start_(0) {}
    void reset()
    {
        active_ = false;
    }
    Gate update(bool held, bool entering, Gate g, const Frame& seat, V hand, V worldAxis, double stroke)
    {
        if (!held || stroke <= .001)
        {
            active_ = false;
            return g;
        }
        const V p = seat.local(hand),
                axis(gx_physics::dot(worldAxis, seat.a), gx_physics::dot(worldAxis, seat.b),
                     gx_physics::dot(worldAxis, seat.c));
        if (!entering)
        {
            active_ = g.at;
            captureHand_ = p;
            start_ = clamp(g.depth, 0, .35);
            if (active_)
                g.depth = start_;
            return g;
        }
        if (active_)
        {
            const V delta = gx_physics::sub(p, captureHand_);
            const double along = gx_physics::dot(delta, axis);
            g.depth = start_ + along / stroke;
            g.lateral = gx_physics::norm(gx_physics::sub(delta, gx_physics::mul(axis, along)));
            g.on = g.lateral <= .11 && g.depth * stroke >= -.07;
        }
        return g;
    }
};
// Round/cylinder seats use their actual radius plus the same 40 mm approach tolerance.
inline Gate seat(V item, V axis, const Frame& frame, V centre, double travel, double dir, double radius)
{
    Gate g;
    if (travel <= .001)
        return g;
    const V p = gx_physics::sub(item, centre);
    const double along = gx_physics::dot(p, frame.a);
    g.lateral = gx_physics::norm(gx_physics::sub(p, gx_physics::mul(frame.a, along)));
    g.depth = (travel + dir * along) / travel;
    g.axis = gx_physics::dot(axis, frame.a);
    g.distance = gx_physics::norm(gx_physics::add(p, gx_physics::mul(frame.a, dir * travel)));
    g.at = g.depth >= -.3 && g.depth <= .4 && g.lateral <= radius + .04 && g.axis >= .7071067811865476;
    // A fast stroke may pass the seat between updates; entry was validated at the mouth.
    g.on = g.depth >= -.5 && g.lateral <= radius + .08;
    return g;
}
inline Gate tube(V tip, V axis, double travel, double radius = .0107)
{
    Frame f;
    return seat(tip, axis, f, V(travel, 0, 0), travel, 1, radius);
}
inline double pull(const Frame& drawn, V captured, V hand)
{
    return captured.x - drawn.local(hand).x;
}
inline bool torso(V handMinusHead)
{
    return handMinusHead.y >= -1.05 && handMinusHead.y <= -.18 &&
           handMinusHead.x * handMinusHead.x + handMinusHead.z * handMinusHead.z <= .30 * .30;
}
inline V rotate(const Frame& f, V p)
{
    return gx_physics::sub(f.world(p), f.p);
}
inline double signedAngle(V axis, V r, V v)
{
    r = gx_physics::sub(r, gx_physics::mul(axis, gx_physics::dot(r, axis)));
    v = gx_physics::sub(v, gx_physics::mul(axis, gx_physics::dot(v, axis)));
    if (gx_physics::dot(r, r) < 1e-8 || gx_physics::dot(v, v) < 1e-8)
        return 0;
    return atan2(gx_physics::dot(gx_hands::cross(r, v), axis), gx_physics::dot(r, v));
}
inline V turn(V axis, V v, double radians)
{
    return gx_physics::add(gx_physics::add(gx_physics::mul(v, cos(radians)),
                                           gx_physics::mul(gx_hands::cross(axis, v), sin(radians))),
                           gx_physics::mul(axis, gx_physics::dot(axis, v) * (1 - cos(radians))));
}
inline bool pinch(const Frame& hand, unsigned side, double reach, Frame& out)
{
    V n = gx_physics::mul(hand.a, side ? -1. : 1.), fingers = gx_physics::mul(hand.b, -1.);
    fingers = gx_physics::sub(fingers, gx_physics::mul(n, gx_physics::dot(fingers, n)));
    const double length = gx_physics::norm(fingers);
    if (length < 1e-4)
        return false;
    fingers = gx_physics::mul(fingers, 1 / length);
    out.a = fingers;
    out.b = n;
    out.c = gx_hands::cross(fingers, n);
    out.p =
        gx_physics::add(hand.p, gx_physics::add(gx_physics::mul(fingers, reach), gx_physics::mul(n, .015)));
    return out.rigid();
}
inline bool magazineInHand(const Frame& hand, unsigned side, V axis, V front, double base, Frame& out)
{
    Frame target;
    if (!pinch(hand, side, 0, target))
        return false;
    const V n = target.b, f = target.a, s = gx_hands::cross(n, f), localSide = gx_hands::cross(axis, front);
    out.a = gx_physics::add(gx_physics::add(gx_physics::mul(n, axis.x), gx_physics::mul(f, front.x)),
                            gx_physics::mul(s, localSide.x));
    out.b = gx_physics::add(gx_physics::add(gx_physics::mul(n, axis.y), gx_physics::mul(f, front.y)),
                            gx_physics::mul(s, localSide.y));
    out.c = gx_physics::add(gx_physics::add(gx_physics::mul(n, axis.z), gx_physics::mul(f, front.z)),
                            gx_physics::mul(s, localSide.z));
    out.p = gx_physics::add(hand.p, gx_physics::mul(n, .022 - base));
    return out.rigid();
}
inline bool magazineShape(V lo, V hi, const Frame& relative, V& axis, V& front, double& base)
{
    axis = gx_physics::mul(gx_physics::add(lo, hi), -.5);
    if (gx_physics::dot(axis, axis) < 1e-4)
        axis = V(0, 0, 1);
    axis = gx_physics::mul(axis, 1 / gx_physics::norm(axis));
    front = V(relative.a.x, relative.b.x, relative.c.x);
    front = gx_physics::sub(front, gx_physics::mul(axis, gx_physics::dot(front, axis)));
    if (gx_physics::dot(front, front) < 1e-4)
        return false;
    front = gx_physics::mul(front, 1 / gx_physics::norm(front));
    base = 0;
    for (unsigned c = 0; c < 8; ++c)
    {
        const V p(c & 1 ? hi.x : lo.x, c & 2 ? hi.y : lo.y, c & 4 ? hi.z : lo.z);
        const double t = gx_physics::dot(p, axis);
        if (t < base)
            base = t;
    }
    return base < -.01 && gx_physics::finite(axis) && gx_physics::finite(front);
}
}
