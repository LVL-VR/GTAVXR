#include "strike_math_contract.h"
#pragma once

// Original GTAVXR CPU math/policy implementation. No PLANCK/HIGGS source used.
// All definitions (including private helpers) are inline for multiple TUs.
namespace gx_strike_math
{
namespace detail
{
inline bool finite(double x)
{
    return gx_physics::finite(x);
}
inline bool finite(V v)
{
    return gx_physics::finite(v);
}
inline double clamp(double x, double lo, double hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}
inline double component(V v, unsigned i)
{
    return i == 0 ? v.x : (i == 1 ? v.y : v.z);
}
inline void component(V& v, unsigned i, double x)
{
    if (i == 0)
        v.x = x;
    else if (i == 1)
        v.y = x;
    else
        v.z = x;
}
// Scaling before squaring avoids overflow in the usual velocity/normal case.
inline bool length(V v, double& out)
{
    out = 0;
    if (!finite(v))
        return false;
    double scale = fabs(v.x);
    if (fabs(v.y) > scale)
        scale = fabs(v.y);
    if (fabs(v.z) > scale)
        scale = fabs(v.z);
    if (scale == 0)
        return true;
    const V q = gx_physics::mul(v, 1 / scale);
    if (!finite(q))
        return false;
    out = scale * sqrt(gx_physics::dot(q, q));
    return finite(out);
}
inline bool unit(V v, V& out)
{
    double n = 0;
    if (!length(v, n) || n <= 1e-12)
        return false;
    out = gx_physics::mul(v, 1 / n);
    return finite(out);
}
inline bool rigid(const Pose& p)
{
    if (!finite(p.p) || !finite(p.a) || !finite(p.b) || !finite(p.c))
        return false;
    const double aa = gx_physics::dot(p.a, p.a), bb = gx_physics::dot(p.b, p.b),
                 cc = gx_physics::dot(p.c, p.c);
    if (!finite(aa) || !finite(bb) || !finite(cc) || fabs(aa - 1) > .01 || fabs(bb - 1) > .01 ||
        fabs(cc - 1) > .01)
        return false;
    return fabs(gx_physics::dot(p.a, p.b)) <= .01 && fabs(gx_physics::dot(p.a, p.c)) <= .01 &&
           fabs(gx_physics::dot(p.b, p.c)) <= .01 && gx_physics::dot(gx_hands::cross(p.a, p.b), p.c) > .99;
}
inline V world(const Pose& p, V v)
{
    return gx_physics::add(
        p.p, gx_physics::add(gx_physics::mul(p.a, v.x),
                             gx_physics::add(gx_physics::mul(p.b, v.y), gx_physics::mul(p.c, v.z))));
}
inline V at(V start, V displacement, double t)
{
    return gx_physics::add(start, gx_physics::mul(displacement, t));
}

// Closest points on closed segments, including point/point and point/segment.
// Overflow in the intermediate geometry fails closed, never returning a NaN hit.
inline bool closest(V p, V q, V a, V b, V& weapon, V& target, double& distance)
{
    const V u = gx_physics::sub(q, p), v = gx_physics::sub(b, a), w = gx_physics::sub(p, a);
    const double uu = gx_physics::dot(u, u), vv = gx_physics::dot(v, v), vw = gx_physics::dot(v, w);
    if (!finite(uu) || !finite(vv) || !finite(vw))
        return false;
    double s = 0, t = 0;
    if (uu <= 1e-24 && vv <= 1e-24)
    {
        weapon = p;
        target = a;
        return length(gx_physics::sub(p, a), distance);
    }
    if (uu <= 1e-24)
        t = clamp(vw / vv, 0, 1);
    else
    {
        const double uw = gx_physics::dot(u, w);
        if (!finite(uw))
            return false;
        if (vv <= 1e-24)
            s = clamp(-uw / uu, 0, 1);
        else
        {
            const V perpendicular = gx_hands::cross(u, v);
            const double uv = gx_physics::dot(u, v), denom = gx_physics::dot(perpendicular, perpendicular);
            if (!finite(uv) || !finite(denom))
                return false;
            if (denom > 0)
                s = clamp(gx_physics::dot(gx_hands::cross(v, w), perpendicular) / denom, 0, 1);
            t = (uv * s + vw) / vv;
            if (!finite(s) || !finite(t))
                return false;
            if (t < 0)
            {
                t = 0;
                s = clamp(-uw / uu, 0, 1);
            }
            else if (t > 1)
            {
                t = 1;
                s = clamp((uv - uw) / uu, 0, 1);
            }
        }
    }
    weapon = at(p, u, s);
    target = at(a, v, t);
    return finite(weapon) && finite(target) && length(gx_physics::sub(weapon, target), distance);
}
inline bool finish(double t, V weapon, V targetPoint, double distance, double combinedRadius,
                   double targetRadius, V gripDelta, V tipDelta, V segment, SweepHit& out)
{
    V normal;
    if (!unit(gx_physics::sub(weapon, targetPoint), normal))
    {
        // Coincident centerlines have no unique geometric normal. Oppose motion;
        // if stationary, choose a reproducible perpendicular, then world +X.
        if (!unit(gx_physics::mul(gx_physics::add(gripDelta, tipDelta), -1), normal))
        {
            const V reference = fabs(segment.z) < fabs(segment.x) + fabs(segment.y) ? V(0, 0, 1) : V(1, 0, 0);
            if (!unit(gx_hands::cross(segment, reference), normal))
                normal = V(1, 0, 0);
        }
    }
    const V point = gx_physics::add(targetPoint, gx_physics::mul(normal, targetRadius));
    if (!finite(point) || !finite(normal))
        return false;
    out.hit = true;
    out.t = clamp(t, 0, 1);
    out.normal = normal;
    out.point = point;
    out.depth = combinedRadius > distance ? combinedRadius - distance : 0;
    return true;
}
inline bool classifierTuning(const ClassifierTuning& t)
{
    return finite(t.swing) && finite(t.stab) && finite(t.punch) && finite(t.minHandSpeed) &&
           finite(t.stabAxisCos) && finite(t.minClosing) && t.swing > 0 && t.stab > 0 &&
           finite(1.5 * t.stab) && t.punch > 0 && t.minHandSpeed >= 0 && t.stabAxisCos >= -1 &&
           t.stabAxisCos <= 1 && t.minClosing >= 0;
}
inline bool gateTuning(const GateTuning& t)
{
    return finite(t.cooldown) && finite(t.separation) && t.cooldown >= 0 && t.separation >= 0;
}
// lastContact >= 0: latest contact time. Negative: -(first absent time + 1).
// inside means the episode has already admitted a strike, not merely overlap.
inline double contactTime(double encoded)
{
    return encoded < 0 ? -encoded - 1 : encoded;
}
inline bool elapsed(double now, double start, double delay)
{
    // A few ulps at the boundary accommodate e.g. (1.12 - 1.0) < .12.
    const double duration = now - start;
    return duration >= delay || (duration >= 0 && delay - duration <= 8 * DBL_EPSILON * (1 + fabs(duration)) +
                                                                          8 * DBL_EPSILON * fabs(delay));
}
}

inline bool WeaponProxy::valid() const
{
    double n = 0;
    return detail::finite(grip) && detail::finite(tip) && detail::finite(radius) && kind <= BLADE &&
           radius >= .01 && radius <= .08 && detail::length(gx_physics::sub(tip, grip), n) && n >= .05 &&
           n <= 1.6;
}

inline bool proxyFromBounds(V boundsMin, V boundsMax, V gripModel, unsigned kind, WeaponProxy& out)
{
    out = WeaponProxy();
    if (!detail::finite(boundsMin) || !detail::finite(boundsMax) || !detail::finite(gripModel) ||
        kind > BLADE)
        return false;
    const V extent = gx_physics::sub(boundsMax, boundsMin);
    if (!detail::finite(extent) || extent.x <= 1e-6 || extent.y <= 1e-6 || extent.z <= 1e-6)
        return false;
    unsigned axis = 0;
    if (extent.y > extent.x)
        axis = 1;
    if (extent.z > detail::component(extent, axis))
        axis = 2;
    // A model wider than the maximum possible grip-to-tip capsule is implausible.
    if (detail::component(extent, axis) > 3.2)
        return false;
    const V center = gx_physics::add(boundsMin, gx_physics::mul(extent, .5));
    if (!detail::finite(center))
        return false;
    double second = 0;
    for (unsigned i = 0; i < 3; ++i)
        if (i != axis && detail::component(extent, i) > second)
            second = detail::component(extent, i);
    WeaponProxy result;
    result.grip = center;
    result.tip = center;
    result.kind = kind;
    const double gripCoordinate = detail::component(gripModel, axis);
    const double lo = detail::component(boundsMin, axis), hi = detail::component(boundsMax, axis);
    const double lowDistance = fabs(gripCoordinate - lo), highDistance = fabs(hi - gripCoordinate);
    if (!detail::finite(lowDistance) || !detail::finite(highDistance))
        return false;
    detail::component(result.grip, axis, gripCoordinate);
    detail::component(result.tip, axis, highDistance >= lowDistance ? hi : lo); // ties choose positive end
    result.radius = detail::clamp(second * .5, .012, .06);
    if (!result.valid())
        return false;
    out = result;
    return true;
}

inline V transformPoint(const Pose& pose, V local)
{
    if (!detail::rigid(pose) || !detail::finite(local))
        return V();
    const V result = detail::world(pose, local);
    return detail::finite(result) ? result : V();
}

inline bool sweep(V gripBefore, V tipBefore, V gripAfter, V tipAfter, double radius, const Capsule& target,
                  SweepHit& out)
{
    out = SweepHit();
    if (!detail::finite(gripBefore) || !detail::finite(tipBefore) || !detail::finite(gripAfter) ||
        !detail::finite(tipAfter) || !detail::finite(radius) || radius <= 0 || !detail::finite(target.a) ||
        !detail::finite(target.b) || !detail::finite(target.r) || target.r <= 0)
        return false;
    const double combined = radius + target.r, tolerance = 1e-8;
    if (!detail::finite(combined))
        return false;
    const V dg = gx_physics::sub(gripAfter, gripBefore), dt = gx_physics::sub(tipAfter, tipBefore);
    double vg = 0, vt = 0;
    if (!detail::length(dg, vg) || !detail::length(dt, vt))
        return false;
    const double travel = vg > vt ? vg : vt;
    double time = 0, distance = 0;
    V weapon, targetPoint;
    // A separating plane at the closest pair remains separating until one
    // endpoint closes its gap. Advancing to that time cannot skip a contact.
    for (unsigned iteration = 0; iteration < 256; ++iteration)
    {
        const V g = detail::at(gripBefore, dg, time), tip = detail::at(tipBefore, dt, time);
        if (!detail::closest(g, tip, target.a, target.b, weapon, targetPoint, distance))
            return false;
        if (distance <= combined + tolerance)
            return detail::finish(time, weapon, targetPoint, distance, combined, target.r, dg, dt,
                                  gx_physics::sub(tip, g), out);
        if (time >= 1 || travel == 0)
            return false;
        V normal;
        if (!detail::unit(gx_physics::sub(weapon, targetPoint), normal))
            return false;
        const double cg = -gx_physics::dot(dg, normal), ct = -gx_physics::dot(dt, normal);
        const double closing = cg > ct ? cg : ct;
        if (!detail::finite(closing) || closing <= 0)
            return false;
        const double step = (distance - combined - tolerance * .5) / closing;
        if (!detail::finite(step) || step > 1 - time)
            return false;
        const double next = time + step * (1 - 16 * DBL_EPSILON);
        if (next <= time)
            break;
        time = next;
    }
    // Bounded, chronological interval fallback for unusually slow convergence.
    // Distance is travel-Lipschitz; midpoint and plane bounds discard only
    // intervals that cannot touch. Earliest remaining interval is visited first.
    struct Interval
    {
        double lo, hi;
        unsigned depth;
    };
    Interval pending[49];
    unsigned count = 1, visits = 0;
    pending[0].lo = time;
    pending[0].hi = 1;
    pending[0].depth = 0;
    while (count && visits < 65536)
    {
        ++visits;
        const Interval interval = pending[--count];
        const double mid = interval.lo + (interval.hi - interval.lo) * .5;
        const V g = detail::at(gripBefore, dg, mid), tip = detail::at(tipBefore, dt, mid);
        if (!detail::closest(g, tip, target.a, target.b, weapon, targetPoint, distance))
            return false;
        const double widthTravel = travel * (interval.hi - interval.lo);
        if (distance - widthTravel * .5 > combined + tolerance)
            continue;
        V normal;
        if (detail::unit(gx_physics::sub(weapon, targetPoint), normal))
        {
            double minimum = DBL_MAX;
            const V endpoints[4] = {
                detail::at(gripBefore, dg, interval.lo), detail::at(tipBefore, dt, interval.lo),
                detail::at(gripBefore, dg, interval.hi), detail::at(tipBefore, dt, interval.hi)};
            for (unsigned i = 0; i < 4; ++i)
            {
                const double projection = gx_physics::dot(gx_physics::sub(endpoints[i], targetPoint), normal);
                if (!detail::finite(projection))
                    return false;
                if (projection < minimum)
                    minimum = projection;
            }
            const double pa = gx_physics::dot(gx_physics::sub(target.a, targetPoint), normal);
            const double pb = gx_physics::dot(gx_physics::sub(target.b, targetPoint), normal);
            if (minimum - (pa > pb ? pa : pb) > combined + tolerance)
                continue;
        }
        if (widthTravel <= tolerance)
        {
            const V firstGrip = detail::at(gripBefore, dg, interval.lo),
                    firstTip = detail::at(tipBefore, dt, interval.lo);
            if (!detail::closest(firstGrip, firstTip, target.a, target.b, weapon, targetPoint, distance))
                return false;
            return detail::finish(interval.lo, weapon, targetPoint, distance, combined, target.r, dg, dt,
                                  gx_physics::sub(firstTip, firstGrip), out);
        }
        if (interval.depth >= 48 || count + 2 > 49)
            return false; // numerical/budget failure stays safe
        pending[count].lo = mid;
        pending[count].hi = interval.hi;
        pending[count++].depth = interval.depth + 1;
        pending[count].lo = interval.lo;
        pending[count].hi = mid;
        pending[count++].depth = interval.depth + 1;
    }
    return false;
}

inline V pointVelocity(V linear, V angular, V center, V point)
{
    if (!detail::finite(linear) || !detail::finite(angular) || !detail::finite(center) ||
        !detail::finite(point))
        return V();
    const V result = gx_physics::add(linear, gx_hands::cross(angular, gx_physics::sub(point, center)));
    return detail::finite(result) ? result : V();
}

inline bool pointVelocityFromPoses(const Pose& before, const Pose& after, V local, double dt, V& out)
{
    out = V();
    if (!detail::rigid(before) || !detail::rigid(after) || !detail::finite(local) || !detail::finite(dt) ||
        dt < .001 || dt > .1)
        return false;
    const V p0 = detail::world(before, local), p1 = detail::world(after, local);
    if (!detail::finite(p0) || !detail::finite(p1))
        return false;
    const V result = gx_physics::mul(gx_physics::sub(p1, p0), 1 / dt);
    if (!detail::finite(result))
        return false;
    out = result;
    return true;
}

inline Classification classify(unsigned weaponKind, V relative, V axis, V normal, double realHandSpeed,
                               const ClassifierTuning& tuning)
{
    Classification out;
    if (weaponKind > BLADE || !detail::finite(relative) || !detail::finite(axis) || !detail::finite(normal) ||
        !detail::finite(realHandSpeed) || realHandSpeed < 0 || !detail::classifierTuning(tuning))
        return out;
    V direction, unitNormal, unitAxis;
    if (!detail::unit(relative, direction) || !detail::unit(normal, unitNormal) ||
        (weaponKind != FIST && !detail::unit(axis, unitAxis)))
        return out;
    double speed = 0;
    if (!detail::length(relative, speed))
        return out;
    const double closing = -gx_physics::dot(relative, unitNormal);
    if (!detail::finite(closing))
        return out;
    out.speed = speed;
    out.closing = closing;
    out.direction = direction;
    if (realHandSpeed < tuning.minHandSpeed || closing < tuning.minClosing)
        return out;
    double threshold = 0, metric = speed;
    if (weaponKind == FIST)
    {
        if (closing < tuning.punch)
            return out;
        out.kind = PUNCH;
        threshold = tuning.punch;
        metric = closing;
    }
    else
    {
        const double stabThreshold = tuning.stab * (weaponKind == BLUNT ? 1.5 : 1);
        if (gx_physics::dot(direction, unitAxis) >= tuning.stabAxisCos && speed >= stabThreshold)
        {
            out.kind = STAB;
            threshold = stabThreshold;
        }
        else if (speed >= tuning.swing)
        {
            out.kind = SWING;
            threshold = tuning.swing;
        }
        else
            return out;
    }
    // Build157: one energy curve; the native caller selects the measured weapon class.
    (void)threshold;
    out.intensity = gx_strike_curve::fraction(metric, weaponKind == FIST ? gx_strike_curve::FIST
                                                      : out.kind == STAB ? gx_strike_curve::KNIFE_STAB
                                                                         : gx_strike_curve::SMALL);
    return out;
}

inline HitGate::HitGate()
{
    reset();
}
inline void HitGate::reset()
{
    for (unsigned i = 0; i < SLOTS; ++i)
    {
        slots_[i].target = 0;
        slots_[i].hand = 0;
        slots_[i].used = false;
        slots_[i].inside = false;
        slots_[i].lastContact = 0;
        slots_[i].lastHit = -1;
    }
}
inline bool HitGate::admit(unsigned hand, uint64_t target, double now, bool contact, bool strike,
                           const GateTuning& tuning)
{
    if (!detail::finite(now) || now < 0 || !detail::gateTuning(tuning))
        return false;
    unsigned index = SLOTS, unused = SLOTS, oldest = 0;
    double oldestTime = DBL_MAX;
    for (unsigned i = 0; i < SLOTS; ++i)
    {
        const Slot& s = slots_[i];
        if (s.used && s.hand == hand && s.target == target)
        {
            index = i;
            break;
        }
        if (!s.used && unused == SLOTS)
            unused = i;
        if (s.used && detail::contactTime(s.lastContact) < oldestTime)
        {
            oldestTime = detail::contactTime(s.lastContact);
            oldest = i;
        }
    }
    if (index == SLOTS)
    {
        if (!contact)
            return false;
        index = unused != SLOTS ? unused : oldest;
        Slot& s = slots_[index];
        s.target = target;
        s.hand = hand;
        s.used = true;
        s.inside = false;
        s.lastContact = now;
        s.lastHit = -1;
    }
    Slot& s = slots_[index];
    const double previous = detail::contactTime(s.lastContact);
    if (now < previous || (s.lastHit >= 0 && now < s.lastHit))
        return false;
    if (!contact)
    {
        if (s.lastContact >= 0)
            s.lastContact = -(now + 1); // start absence at first observed false, not last true
        if (detail::elapsed(now, detail::contactTime(s.lastContact), tuning.separation))
            s.inside = false;
        return false;
    }
    if (s.lastContact < 0 && detail::elapsed(now, previous, tuning.separation))
        s.inside = false;
    s.lastContact = now;
    if (!strike || s.inside || (s.lastHit >= 0 && !detail::elapsed(now, s.lastHit, tuning.cooldown)))
        return false;
    s.lastHit = now;
    s.inside = true;
    return true;
}
inline void HitGate::forgetHand(unsigned hand)
{
    for (unsigned i = 0; i < SLOTS; ++i)
        if (slots_[i].used && slots_[i].hand == hand)
            slots_[i].used = false;
}

inline ResponsePlan respond(const Classification& c, bool power, double massFactor,
                            const ResponseTuning& tuning)
{
    ResponsePlan out;
    if (c.kind < SWING || c.kind > PUNCH || !detail::finite(c.speed) || c.speed < 0 ||
        !detail::finite(c.closing) || c.closing < 0 || !detail::finite(c.intensity) ||
        !detail::finite(c.direction) || !detail::finite(massFactor) || !detail::finite(tuning.stagger) ||
        !detail::finite(tuning.ragdoll) || !detail::finite(tuning.impulseMin) ||
        !detail::finite(tuning.impulseMax) || tuning.stagger < 0 || tuning.ragdoll < tuning.stagger ||
        tuning.ragdoll > 1 || tuning.impulseMin < 0 || tuning.impulseMax < tuning.impulseMin)
        return out;
    V direction;
    if (!detail::unit(c.direction, direction))
        return out;
    const double intensity = detail::clamp(c.intensity, 0, 1), mass = detail::clamp(massFactor, .5, 1.5);
    const double base = tuning.impulseMin + (tuning.impulseMax - tuning.impulseMin) * intensity;
    out.impulse = mass > 1 && base > tuning.impulseMax / mass ? tuning.impulseMax : base * mass;
    if (!detail::finite(out.impulse))
        return ResponsePlan();
    out.kind = power
                   ? RAGDOLL
                   : (intensity < tuning.stagger ? FLINCH : (intensity < tuning.ragdoll ? STAGGER : RAGDOLL));
    out.direction = direction;
    return out;
}
}
