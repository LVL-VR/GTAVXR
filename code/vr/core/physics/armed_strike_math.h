#pragma once
// Build142 F4 CONTRACT (frozen by Claude before delegation; implementation goes in armed_strike_math.h, which includes
// strike_math.h and only ADDS to it). Pure math/policy, VS2012-compatible (no C++14), NO game/runtime state, NO
// PLANCK/HIGGS code. Units: metres, seconds, m/s, rad/s.
//
 
// (gx_hand_motion::pointMotion valid only |v|<=12, gx_physical138::speedStrike own<=12 / relative<=14 m/s and the
// free-endpoint gx_strike::Episode), so fast bat-tip swings were discarded and a NPC kept against the arm never
// "separated". 16 swept bat hits -> 0 damage. The knife never produced a contact (likely no phBound leaf).
// 142 glue will use gx_planck141::classify + gx_planck141::HitGate for weapons and the additions below.
#include "strike_math.h"
namespace gx_armed_strike_math
{
using gx_physics::V;
using gx_strike_math::Pose;
using gx_strike_math::Capsule;
using gx_strike_math::SweepHit;

// ---------------------------------------------------------------- weapon part as an oriented box
// Box in its OWN frame: centre c, unit orthogonal axes u[3] (right-handed), half extents h[3] (>= 0; a zero extent is a
// flat plate, still valid). Coordinates are in the frame the caller states per function.
struct Box
{
    V c, u[3];
    double h[3];
    Box()
    {
        h[0] = h[1] = h[2] = 0;
    }
    bool valid() const;
};
// Axis-aligned bounds -> Box (axes = identity). Used for (a) each phBound leaf AABB in the leaf frame and (b) the
// FALLBACK when the held weapon object has no physics leaf: the drawable/model AABB. Reject non-finite, any half
// extent > 1.0 m, or all three half extents < 0.003 m. A thin blade (one tiny extent) MUST be accepted.
inline bool boxFromBounds(V lo, V hi, Box& out);
// Rigid map of a Box by a Pose (rotation rows a,b,c + origin p, same convention as gx_planck141::Pose / transformPoint).
inline Box transformBox(const Pose& pose, const Box& local);

// ---------------------------------------------------------------- relative box sweep
// The target ragdoll component is a capsule `target` in ITS OWN component frame. box0/box1 are the weapon part at the
// previous/current sample, ALREADY expressed in that component frame (caller maps them with the component's previous/
// current pose). The box moves between them with linearly interpolated centre and normalised-lerp axes.
// Requirements:
//  - First contact time t in [0,1] wins; point = on the target surface, normal = unit, from target towards the box,
//    depth >= 0 (0 allowed at exact touch).
//  - No tunnelling: a box with 0.002 m minimum half extent (blade) must not pass a 0.04 m-radius capsule for relative
//    point speeds up to 40 m/s and rotation up to 0.6 rad between samples (substep or conservative advancement; state
//    the bound you prove in DELIVERY.md).
//  - No phantom: if the box never comes within (target.r - 0.001) of the capsule axis at ANY t of the interpolated
//    motion, return false. Rotation of box0->box1 above 0.6 rad -> return false (unqualified, caller falls back to
//    the end-of-frame overlap witness).
//  - Pure and deterministic; bounded iterations (state the max).
inline bool sweepBox(const Box& box0, const Box& box1, const Capsule& target, SweepHit& out);

// ---------------------------------------------------------------- weapon point motion validity
// Build157: hands and weapon points share gx_motion::GLITCH. Lever arm and legacy tuning
// are validated for API compatibility; they no longer impose a second speed limit.
struct MotionTuning
{
    double handLimit, omegaMax, absoluteMax;
    MotionTuning() : handLimit(gx_motion::GLITCH), omegaMax(40), absoluteMax(gx_motion::GLITCH) {}
};
inline bool weaponPointMotionValid(V pointVelocity, double leverArm, const MotionTuning& tuning);

// ---------------------------------------------------------------- per-gate telemetry model
// Every armed update ends in exactly ONE reason. Counted per hand and per weapon hash by the glue; logged with the
// weapon hash so a worn run tells which gate rejected which weapon.
enum Gate
{
    G_NO_WEAPON_SHAPE = 0, // no phBound leaf AND no drawable bounds -> nothing to test
    G_FALLBACK_SHAPE,      // informational: drawable fallback box used this update (counted in addition)
    G_CLEAR,               // no overlap, no sweep hit
    G_DISCONTINUOUS,       // pose history not continuous (tracking/time gap)
    G_SWEEP_UNQUALIFIED,   // rotation gate / invalid input -> sweep skipped
    G_MOTION_INVALID,      // weaponPointMotionValid false
    G_NOT_A_STRIKE,        // classify -> NO_STRIKE (too slow / not closing)
    G_EPISODE_REFUSED,     // HitGate: same contact episode or cooldown
    G_REVALIDATION_FAILED, // final native revalidation (target/pose/input changed)
    G_DAMAGE,              // dispatched to native damage
    G_COUNT
};
inline const char* gateName(unsigned gate); // stable lower_snake names, "unknown" when out of range
}

// Original CPU geometry and policy. The frozen contract above is verbatim.
// No external physics implementation, allocation, mutable static, or game state.
namespace gx_armed_strike_math
{
namespace detail
{
using gx_physics::add;
using gx_physics::sub;
using gx_physics::mul;
using gx_physics::dot;
using gx_hands::cross;
using gx_strike_math::detail::finite;
using gx_strike_math::detail::length;
using gx_strike_math::detail::unit;
using gx_strike_math::detail::clamp;

inline V vectorMap(const Pose& p, V v)
{
    return add(mul(p.a, v.x), add(mul(p.b, v.y), mul(p.c, v.z)));
}
inline V at(V a, V b, double t)
{
    return add(mul(a, 1 - t), mul(b, t));
}

// Nlerping each axis is the literal contract, not a quaternion interpolation.
// For a general rotation the intermediate axes need not be orthogonal. The
// exact convex solid is c + sum(u[i]*s[i]), -h[i] <= s[i] <= h[i].
inline bool interpolate(const Box& a, const Box& b, double t, Box& out)
{
    out.c = at(a.c, b.c, t);
    if (!finite(out.c))
        return false;
    for (unsigned i = 0; i < 3; ++i)
    {
        if (!unit(at(a.u[i], b.u[i], t), out.u[i]))
            return false;
        out.h[i] = a.h[i];
    }
    return true;
}

inline bool remember(V weapon, V target, V& bestWeapon, V& bestTarget, double& best)
{
    double d = 0;
    if (!length(sub(weapon, target), d))
        return false;
    if (d < best)
    {
        best = d;
        bestWeapon = weapon;
        bestTarget = target;
    }
    return true;
}

// Exact segment/convex-box distance: slab intersection, the twelve edges,
// and projections of both segment endpoints into each face interior.
// This also works with one, two, or three zero half extents and skew axes.
// If a closest segment/face pair has neither point on a boundary, the segment
// is parallel to that face. An endpoint projection or a face edge then attains
// the same distance, so these candidates are exhaustive.
inline bool distance(const Box& box, V a, V b, V& weapon, V& target, double& result)
{
    const V dual[3] = {cross(box.u[1], box.u[2]), cross(box.u[2], box.u[0]), cross(box.u[0], box.u[1])};
    const double determinant = dot(box.u[0], dual[0]);
    if (!finite(determinant) || determinant <= 1e-12)
        return false;
    const V ac = sub(a, box.c), bc = sub(b, box.c);
    double enter = 0, leave = 1;
    bool intersects = true;
    for (unsigned i = 0; i < 3; ++i)
    {
        const double p = dot(ac, dual[i]) / determinant, q = dot(bc, dual[i]) / determinant;
        const double delta = q - p;
        if (!finite(p) || !finite(q) || !finite(delta))
            return false;
        if (delta == 0)
        {
            if (p < -box.h[i] || p > box.h[i])
                intersects = false;
        }
        else
        {
            double lo = (-box.h[i] - p) / delta, hi = (box.h[i] - p) / delta;
            if (lo > hi)
            {
                const double temp = lo;
                lo = hi;
                hi = temp;
            }
            if (lo > enter)
                enter = lo;
            if (hi < leave)
                leave = hi;
            if (enter > leave)
                intersects = false;
        }
    }
    if (intersects)
    {
        target = at(a, b, clamp(enter, 0, 1));
        weapon = target;
        result = 0;
        return finite(target);
    }
    result = DBL_MAX;
    for (unsigned i = 0; i < 3; ++i)
    {
        const unsigned j = (i + 1) % 3, k = (i + 2) % 3;
        for (unsigned bits = 0; bits < 4; ++bits)
        {
            const V center = add(box.c, add(mul(box.u[j], (bits & 1) ? box.h[j] : -box.h[j]),
                                            mul(box.u[k], (bits & 2) ? box.h[k] : -box.h[k])));
            const V p = sub(center, mul(box.u[i], box.h[i])), q = add(center, mul(box.u[i], box.h[i]));
            V w, t;
            double d = 0;
            if (!gx_strike_math::detail::closest(p, q, a, b, w, t, d))
                return false;
            if (d < result)
            {
                result = d;
                weapon = w;
                target = t;
            }
        }
        V normal;
        if (!unit(dual[i], normal))
            return false;
        const double jj = dot(box.u[j], box.u[j]), kk = dot(box.u[k], box.u[k]), jk = dot(box.u[j], box.u[k]);
        const double gram = jj * kk - jk * jk;
        if (!finite(gram) || gram <= 1e-12)
            return false;
        for (unsigned side = 0; side < 2; ++side)
        {
            const V face = add(box.c, mul(box.u[i], side ? box.h[i] : -box.h[i]));
            for (unsigned endpoint = 0; endpoint < 2; ++endpoint)
            {
                const V p = endpoint ? b : a, offset = sub(p, face);
                const V projection = sub(p, mul(normal, dot(offset, normal))), v = sub(projection, face);
                const double vj = dot(v, box.u[j]), vk = dot(v, box.u[k]);
                const double sj = (vj * kk - vk * jk) / gram, sk = (vk * jj - vj * jk) / gram;
                if (!finite(sj) || !finite(sk))
                    return false;
                if (fabs(sj) <= box.h[j] && fabs(sk) <= box.h[k] &&
                    !remember(projection, p, weapon, target, result))
                    return false;
            }
        }
    }
    return finite(result) && result < DBL_MAX;
}

// Bound |n dot normalized(lerp(u0,u1,t))| on an interval. Its derivative
// has a linear numerator, so endpoints and at most one stationary point
// exhaust the extrema. Scaling tolerances cover a few floating point ulps.
inline bool axisSupport(V u0, V u1, V n, double lo, double hi, double& out)
{
    const V delta = sub(u1, u0);
    const double a = dot(n, u0), b = dot(n, delta), c = dot(u0, u0), d = 2 * dot(u0, delta),
                 e = dot(delta, delta);
    const double numerator0 = b * c - a * d * .5, numerator1 = b * d * .5 - a * e;
    double times[3] = {lo, hi, lo};
    unsigned count = 2;
    if (numerator1 != 0)
    {
        const double t = -numerator0 / numerator1;
        if (t > lo && t < hi)
            times[count++] = t;
    }
    out = 0;
    for (unsigned i = 0; i < count; ++i)
    {
        V axis;
        if (!unit(at(u0, u1, times[i]), axis))
            return false;
        const double projection = fabs(dot(n, axis));
        if (!finite(projection))
            return false;
        if (projection > out)
            out = projection;
    }
    return finite(numerator0) && finite(numerator1);
}

// Separating-plane lower bound for the entire moving convex box. Individual
// support maxima may occur at different times; that only weakens the bound.
inline bool planeClear(const Box& a, const Box& b, const Capsule& capsule, V n, double lo, double hi,
                       double radius, bool& clear)
{
    // Use capsule.a as the origin to reduce cancellation in component frames.
    const double p = dot(sub(at(a.c, b.c, lo), capsule.a), n), q = dot(sub(at(a.c, b.c, hi), capsule.a), n);
    const double target = dot(sub(capsule.b, capsule.a), n);
    double lower = (p < q ? p : q) - (target > 0 ? target : 0), scale = 1 + fabs(p) + fabs(q) + fabs(target);
    for (unsigned i = 0; i < 3; ++i)
    {
        double support = 0;
        if (!axisSupport(a.u[i], b.u[i], n, lo, hi, support))
            return false;
        const double term = a.h[i] * support;
        lower -= term;
        scale += fabs(term);
    }
    if (!finite(lower) || !finite(scale))
        return false;
    clear = lower > radius + 128 * DBL_EPSILON * scale;
    return true;
}

enum
{
    MAX_VISITS = 65536,
    MAX_DEPTH = 52,
    MAX_CERTIFICATE_STEPS = 65536
};
inline double contactTolerance()
{
    return 1e-8;
} // metres, not a radius inflation used to qualify a sweep

// Chronological depth-first intervals. Every discard has a distance-Lipschitz
// or a swept separating-plane certificate. The strict pass requires an actual
// inner-radius witness; the surface pass resolves first contact to 1e-8 m.
inline bool first(const Box& a, const Box& b, const Capsule& capsule, double radius, double travel,
                  double end, bool strict, double& time, V& weapon, V& target, double& d)
{
    struct Interval
    {
        double lo, hi;
        unsigned depth;
    };
    Interval stack[MAX_DEPTH + 1];
    unsigned count = 1, visits = 0;
    stack[0].lo = 0;
    stack[0].hi = end;
    stack[0].depth = 0;
    while (count && visits < MAX_VISITS)
    {
        ++visits;
        const Interval interval = stack[--count];
        const double mid = interval.lo + (interval.hi - interval.lo) * .5;
        Box box;
        if (!interpolate(a, b, mid, box) || !distance(box, capsule.a, capsule.b, weapon, target, d))
            return false;
        const double span = travel * (interval.hi - interval.lo),
                     roundoff = 128 * DBL_EPSILON * (1 + d + span);
        if (!finite(span) || !finite(roundoff))
            return false;
        if (d - span * .5 > radius + roundoff)
            continue;
        V normal;
        if (unit(sub(weapon, target), normal))
        {
            bool clear = false;
            if (!planeClear(a, b, capsule, normal, interval.lo, interval.hi, radius, clear))
                return false;
            if (clear)
                continue;
        }
        if (strict && d <= radius)
        {
            time = mid;
            return true;
        }
        if (span <= contactTolerance() || mid == interval.lo || mid == interval.hi)
        {
            if (!interpolate(a, b, interval.lo, box) ||
                !distance(box, capsule.a, capsule.b, weapon, target, d))
                return false;
            if (d <= radius + (strict ? 0 : contactTolerance()))
            {
                time = interval.lo;
                return true;
            }
            if (!interpolate(a, b, interval.hi, box) ||
                !distance(box, capsule.a, capsule.b, weapon, target, d))
                return false;
            if (d <= radius + (strict ? 0 : contactTolerance()))
            {
                time = interval.hi;
                return true;
            }
            continue;
        }
        if (interval.depth >= MAX_DEPTH || count + 2 > MAX_DEPTH + 1)
            return false;
        stack[count].lo = mid;
        stack[count].hi = interval.hi;
        stack[count++].depth = interval.depth + 1;
        stack[count].lo = interval.lo;
        stack[count].hi = mid;
        stack[count++].depth = interval.depth + 1;
    }
    return false;
}

// Independent of adaptive convergence: uniform strict witnesses with <=1 mm
// point travel. If min distance <= r-1.5 mm, a sample proves <= r-1 mm.
// This is used only when the adaptive strict search finds no witness.
inline bool certificate(const Box& a, const Box& b, const Capsule& capsule, double radius, double travel,
                        double& time)
{
    if (travel > MAX_CERTIFICATE_STEPS * .001)
        return false;
    unsigned steps = static_cast<unsigned>(ceil(travel / .001));
    if (steps < 1)
        steps = 1;
    for (unsigned i = 0; i <= steps; ++i)
    {
        const double t = static_cast<double>(i) / steps;
        Box box;
        V weapon, target;
        double d = 0;
        if (!interpolate(a, b, t, box) || !distance(box, capsule.a, capsule.b, weapon, target, d))
            return false;
        if (d <= radius)
        {
            time = t;
            return true;
        }
    }
    return false;
}

inline bool finish(double t, V weapon, V target, double distance, const Capsule& capsule, V delta,
                   SweepHit& out)
{
    V normal;
    if (!unit(sub(weapon, target), normal))
    {
        // An axis inside the box has no unique nearest normal. Choose a unit
        // lateral capsule normal, preferring the direction opposed to motion.
        V axis;
        if (unit(sub(capsule.b, capsule.a), axis))
        {
            V desired = mul(delta, -1);
            desired = sub(desired, mul(axis, dot(desired, axis)));
            if (!unit(desired, normal))
            {
                const V reference = fabs(axis.x) < .8 ? V(1, 0, 0) : V(0, 1, 0);
                if (!unit(cross(axis, reference), normal))
                    return false;
            }
        }
        else if (!unit(mul(delta, -1), normal))
            normal = V(1, 0, 0);
    }
    const V point = add(target, mul(normal, capsule.r));
    if (!finite(point) || !finite(normal))
        return false;
    out.hit = true;
    out.t = clamp(t, 0, 1);
    out.point = point;
    out.normal = normal;
    out.depth = distance < capsule.r ? capsule.r - distance : 0;
    return true;
}
}

inline bool Box::valid() const
{
    if (!detail::finite(c))
        return false;
    for (unsigned i = 0; i < 3; ++i)
    {
        if (!detail::finite(u[i]) || !detail::finite(h[i]) || h[i] < 0 ||
            fabs(detail::dot(u[i], u[i]) - 1) > 1e-6)
            return false;
    }
    return fabs(detail::dot(u[0], u[1])) <= 1e-6 && fabs(detail::dot(u[0], u[2])) <= 1e-6 &&
           fabs(detail::dot(u[1], u[2])) <= 1e-6 && detail::dot(detail::cross(u[0], u[1]), u[2]) >= 1 - 3e-6;
}

inline bool boxFromBounds(V lo, V hi, Box& out)
{
    out = Box();
    if (!detail::finite(lo) || !detail::finite(hi))
        return false;
    Box result;
    result.c = detail::add(detail::mul(lo, .5), detail::mul(hi, .5));
    result.u[0] = V(1, 0, 0);
    result.u[1] = V(0, 1, 0);
    result.u[2] = V(0, 0, 1);
    const double low[3] = {lo.x, lo.y, lo.z}, high[3] = {hi.x, hi.y, hi.z};
    bool substantial = false;
    for (unsigned i = 0; i < 3; ++i)
    {
        if (low[i] > high[i])
            return false;
        result.h[i] = high[i] * .5 - low[i] * .5;
        if (!detail::finite(result.h[i]) || result.h[i] > 1)
            return false;
        if (result.h[i] >= .003)
            substantial = true;
    }
    if (!substantial || !result.valid())
        return false;
    out = result;
    return true;
}

inline Box transformBox(const Pose& pose, const Box& local)
{
    Box out;
    if (!local.valid() || !gx_strike_math::detail::rigid(pose))
        return out;
    out.c = detail::add(pose.p, detail::vectorMap(pose, local.c));
    for (unsigned i = 0; i < 3; ++i)
    {
        out.u[i] = detail::vectorMap(pose, local.u[i]);
        out.h[i] = local.h[i];
    }
    return out.valid() ? out : Box();
}

inline bool sweepBox(const Box& box0, const Box& box1, const Capsule& target, SweepHit& out)
{
    out = SweepHit();
    if (!box0.valid() || !box1.valid() || !detail::finite(target.a) || !detail::finite(target.b) ||
        !detail::finite(target.r) || target.r < .001)
        return false;
    // One immutable part is swept. The frozen contract specifies no extent
    // interpolation for different shapes, so a shape change is unqualified.
    for (unsigned i = 0; i < 3; ++i)
        if (box0.h[i] != box1.h[i])
            return false;
    double trace = 0;
    for (unsigned i = 0; i < 3; ++i)
        trace += detail::dot(box0.u[i], box1.u[i]);
    if (!detail::finite(trace) || detail::clamp((trace - 1) * .5, -1, 1) < cos(.6) - 16 * DBL_EPSILON)
        return false;
    const V delta = detail::sub(box1.c, box0.c);
    double travel = 0;
    if (!detail::length(delta, travel))
        return false;
    for (unsigned i = 0; i < 3; ++i)
    {
        const V d = detail::sub(box1.u[i], box0.u[i]);
        double speed = 0;
        if (!detail::length(d, speed))
            return false;
        const double aa = detail::dot(box0.u[i], box0.u[i]), dd = detail::dot(d, d);
        const double t = dd > 0 ? detail::clamp(-detail::dot(box0.u[i], d) / dd, 0, 1) : 0;
        const double minimum = aa + 2 * t * detail::dot(box0.u[i], d) + t * t * dd;
        if (!detail::finite(minimum) || minimum <= 0)
            return false;
        travel += box0.h[i] * speed / sqrt(minimum);
    }
    // Inflate the derivative upper bound by roundoff, never the qualification radius.
    travel *= 1 + 128 * DBL_EPSILON;
    if (!detail::finite(travel))
        return false;
    V weapon, point;
    double distance = 0;
    if (!detail::distance(box0, target.a, target.b, weapon, point, distance))
        return false;
    const double initial = distance, inner = target.r - .001;
    double witness = 0;
    if (initial > inner)
    {
        if (travel == 0)
            return false;
        if (!detail::first(box0, box1, target, inner, travel, 1, true, witness, weapon, point, distance) &&
            !detail::certificate(box0, box1, target, inner, travel, witness))
            return false;
    }
    if (initial <= target.r)
    {
        if (!detail::distance(box0, target.a, target.b, weapon, point, distance))
            return false;
        return detail::finish(0, weapon, point, distance, target, delta, out);
    }
    bool translation = true;
    for (unsigned i = 0; i < 3; ++i)
        if (box0.u[i].x != box1.u[i].x || box0.u[i].y != box1.u[i].y || box0.u[i].z != box1.u[i].z)
            translation = false;
    if (translation)
    {
        // Distance between two convex sets under translation is convex in t.
        // Its radius sublevel set is an interval. A strict inner witness is
        // therefore a right bracket for the unique first-contact boundary.
        double lo = 0, hi = witness;
        for (unsigned iteration = 0; iteration < 60; ++iteration)
        {
            const double mid = lo + (hi - lo) * .5;
            if (mid == lo || mid == hi)
                break;
            Box box;
            if (!detail::interpolate(box0, box1, mid, box) ||
                !detail::distance(box, target.a, target.b, weapon, point, distance))
                return false;
            if (distance <= target.r)
                hi = mid;
            else
                lo = mid;
        }
        Box box;
        if (!detail::interpolate(box0, box1, hi, box) ||
            !detail::distance(box, target.a, target.b, weapon, point, distance))
            return false;
        return detail::finish(hi, weapon, point, distance, target, delta, out);
    }
    double time = 0;
    if (!detail::first(box0, box1, target, target.r, travel, witness, false, time, weapon, point, distance))
        return false;
    return detail::finish(time, weapon, point, distance, target, delta, out);
}

inline bool weaponPointMotionValid(V pointVelocity, double leverArm, const MotionTuning& tuning)
{
    if (!detail::finite(leverArm) || leverArm < 0 || !detail::finite(tuning.handLimit) ||
        tuning.handLimit < 0 || !detail::finite(tuning.omegaMax) || tuning.omegaMax < 0 ||
        !detail::finite(tuning.absoluteMax) || tuning.absoluteMax < 0)
        return false;
    double speed = 0;
    if (!detail::length(pointVelocity, speed))
        return false;
    return speed <= gx_motion::GLITCH;
}

inline const char* gateName(unsigned gate)
{
    switch (gate)
    {
    case G_NO_WEAPON_SHAPE:
        return "no_weapon_shape";
    case G_FALLBACK_SHAPE:
        return "fallback_shape";
    case G_CLEAR:
        return "clear";
    case G_DISCONTINUOUS:
        return "discontinuous";
    case G_SWEEP_UNQUALIFIED:
        return "sweep_unqualified";
    case G_MOTION_INVALID:
        return "motion_invalid";
    case G_NOT_A_STRIKE:
        return "not_a_strike";
    case G_EPISODE_REFUSED:
        return "episode_refused";
    case G_REVALIDATION_FAILED:
        return "revalidation_failed";
    case G_DAMAGE:
        return "damage";
    default:
        return "unknown";
    }
}
}
