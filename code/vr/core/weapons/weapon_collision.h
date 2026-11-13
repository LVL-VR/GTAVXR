#pragma once
 
// traspasaba las paredes ... las colisiones con el mundo ... no están funcionando como funcionaban en ... las manos").
// Pure solver (CPU-tested): the held weapon's REAL part capsules (weapon-root local frame) are swept from the previous SOLVED
// pose to the desired (hand-driven) pose. The earliest BLOCKING hit (map/buildings, vehicles, fixed or heavy objects) stops
// the weapon just before it (position AND rotation interpolated, like the arm stopping at a wall); LIGHT movable props never
// block (they are pushed/struck by the caller); peds are IGNORED (the armed NPC path owns them).
// build151: conservative spatial/rotational coverage, explicit unavailable outcomes, no lag snap through geometry.
#include <math.h>
#include "../physics/physics_contact.h"
namespace gx_wcol
{
using gx_physics::V;
using gx_physics::Frame;
using gx_physics::Hit;
enum Kind
{
    IGNORE_HIT,
    LIGHT,
    BLOCK
};
struct Capsule
{
    V a, b;
    double r;
    Capsule() : r(0) {}
};
inline double clamp01(double t)
{
    return t < 0 ? 0 : (t > 1 ? 1 : t);
}
 
inline void toQuat(const Frame& f, double q[4])
{
    const double m00 = f.a.x, m01 = f.a.y, m02 = f.a.z, m10 = f.b.x, m11 = f.b.y, m12 = f.b.z, m20 = f.c.x,
                 m21 = f.c.y, m22 = f.c.z, tr = m00 + m11 + m22;
    if (tr > 0)
    {
        const double s = sqrt(tr + 1) * 2;
        q[3] = .25 * s;
        q[0] = (m12 - m21) / s;
        q[1] = (m20 - m02) / s;
        q[2] = (m01 - m10) / s;
    }
    else if (m00 > m11 && m00 > m22)
    {
        const double s = sqrt(1 + m00 - m11 - m22) * 2;
        q[3] = (m12 - m21) / s;
        q[0] = .25 * s;
        q[1] = (m01 + m10) / s;
        q[2] = (m20 + m02) / s;
    }
    else if (m11 > m22)
    {
        const double s = sqrt(1 + m11 - m00 - m22) * 2;
        q[3] = (m20 - m02) / s;
        q[0] = (m01 + m10) / s;
        q[1] = .25 * s;
        q[2] = (m12 + m21) / s;
    }
    else
    {
        const double s = sqrt(1 + m22 - m00 - m11) * 2;
        q[3] = (m01 - m10) / s;
        q[0] = (m20 + m02) / s;
        q[1] = (m12 + m21) / s;
        q[2] = .25 * s;
    }
}
inline Frame fromQuat(const double q[4], V p)
{
    const double x = q[0], y = q[1], z = q[2], w = q[3];
    Frame f;
    f.a = V(1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w));
    f.b = V(2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w));
    f.c = V(2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y));
    f.p = p;
    return f;
}
// Pose between two rigid frames: translation lerp + constant-speed rotation along the short arc.
inline Frame between(const Frame& p, const Frame& d, double t)
{
    t = clamp01(t);
    if (t <= 0)
        return p;
    if (t >= 1)
        return d;
    double a[4], b[4];
    toQuat(p, a);
    toQuat(d, b);
    double dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if (dot < 0)
    {
        for (unsigned i = 0; i < 4; ++i)
            b[i] = -b[i];
        dot = -dot;
    }
    if (dot > 1)
        dot = 1;
    const double angle = acos(dot), denom = sin(angle);
    const double wa = denom > 1e-8 ? sin((1 - t) * angle) / denom : 1 - t,
                 wb = denom > 1e-8 ? sin(t * angle) / denom : t;
    double q[4], n = 0;
    for (unsigned i = 0; i < 4; ++i)
    {
        q[i] = wa * a[i] + wb * b[i];
        n += q[i] * q[i];
    }
    n = sqrt(n);
    if (!(n > 1e-9))
        return d;
    for (unsigned i = 0; i < 4; ++i)
        q[i] /= n;
    return fromQuat(q, gx_physics::add(p.p, gx_physics::mul(gx_physics::sub(d.p, p.p), t)));
}
// X expressed relative to `from`, carried rigidly onto `to` (the grip/hand rides the solved weapon).
inline Frame carry(const Frame& from, const Frame& to, const Frame& x)
{
    Frame out;
    out.p = to.world(from.local(x.p));
    const Frame* f = &from;
    const V axes[3] = {x.a, x.b, x.c};
    V moved[3];
    for (unsigned i = 0; i < 3; ++i)
    {
        const V l(gx_physics::dot(axes[i], f->a), gx_physics::dot(axes[i], f->b),
                  gx_physics::dot(axes[i], f->c));
        moved[i] = gx_physics::add(gx_physics::mul(to.a, l.x),
                                   gx_physics::add(gx_physics::mul(to.b, l.y), gx_physics::mul(to.c, l.z)));
    }
    out.a = moved[0];
    out.b = moved[1];
    out.c = moved[2];
    return out;
}
inline Frame holdingHand(const Frame& desired, const Frame& solved, const Frame& tracked)
{
    const Frame fixed = carry(desired, solved, tracked);
    return gx_physics::norm(gx_physics::sub(fixed.p, tracked.p)) <= .80 ? fixed : tracked;
}
enum
{
    MAX_CAPS = 16,
    MAX_POINTS = 512,
    MAX_STEPS = 64,
    MAX_QUERIES = 2048
};
// at most 4 mm of extra radius for complete spatial capsule coverage
inline double SPACE_PAD()
{
    return .004;
}
// at most 2 mm of extra radius for arc-to-chord error
inline double ARC_PAD()
{
    return .002;
}
// Endpoints plus uniform interior samples. R=sqrt(r*r+s*s/4) covers EVERY point of the original capsule:
// the nearest sample is <=s/2 axially away, with radial distance <=r. No truncation: capacity -> unavailable.
inline unsigned points(const Capsule* c, unsigned n, V* out, double* radius, unsigned cap)
{
    if (!c || !n || n > MAX_CAPS)
        return 0;
    unsigned k = 0;
    for (unsigned i = 0; i < n; ++i)
    {
        const double len = gx_physics::norm(gx_physics::sub(c[i].b, c[i].a)), r = c[i].r;
        if (!gx_physics::finite(c[i].a) || !gx_physics::finite(c[i].b) || !gx_physics::finite(len) ||
            !gx_physics::finite(r) || r <= 0)
            return 0;
        const double spacing = 2 * sqrt(2 * r * SPACE_PAD() + SPACE_PAD() * SPACE_PAD());
        const double required = ceil(len / spacing);
        if (required > cap)
            return 0;
        const unsigned m = required > 1 ? (unsigned)required : 1;
        if (k + m + 1 > cap)
            return 0;
        const double s = len / m, R = sqrt(r * r + s * s * .25);
        for (unsigned j = 0; j <= m; ++j)
        {
            out[k] = gx_physics::add(c[i].a, gx_physics::mul(gx_physics::sub(c[i].b, c[i].a), (double)j / m));
            radius[k] = R;
            ++k;
        }
    }
    return k;
}
// The nearest BLOCK and nearest LIGHT are independent, in either native result order.
struct Contacts
{
    Hit block, light;
    void add(const Hit& hit, Kind kind)
    {
        if (!hit.present || !hit.usable())
            return;
        if (kind == BLOCK && (!block.present || hit.fraction < block.fraction))
            block = hit;
        if (kind == LIGHT && (!light.present || hit.fraction < light.fraction))
            light = hit;
    }
};
// Jam escape is presentation-only and unavailable for impacts. A collision-constrained
// BLOCK never becomes ghosted from pressure. Recovery proves a swept reachable path.
enum Outcome
{
    UNAVAILABLE,
    FREE,
    BLOCKED,
    INITIAL_OVERLAP,
    RECOVERY_LIMIT,
    RECOVERED,
    QUERY_FAILED,
    CAPACITY,
    ESCAPED
};
struct Result
{
    bool valid, blocked, light, unavailable;
    Frame solved;
    double fraction;
    Hit block, lightHit;
    unsigned queries, failures;
    Outcome outcome;
    Result()
        : valid(false), blocked(false), light(false), unavailable(true), fraction(1), queries(0), failures(0),
          outcome(UNAVAILABLE)
    {
    }
};
class Solver
{
    Frame prev_;
    bool ready_, anchored_;
    uint64_t object_;
    // Only a proven initial overlap can enter jam recovery. BLOCKED pressure is never a jam.
    bool jam_, ghost_;
    double jamTime_, away_;
    V jamNormal_;
    static double angle(const Frame& a, const Frame& b)
    {
        double qa[4], qb[4];
        toQuat(a, qa);
        toQuat(b, qb);
        double d = fabs(qa[0] * qb[0] + qa[1] * qb[1] + qa[2] * qb[2] + qa[3] * qb[3]);
        return 2 * acos(d > 1 ? 1 : d);
    }
    static double skinFraction(const Frame& a, const Frame& b, double reach)
    {
        const double travel = gx_physics::norm(gx_physics::sub(b.p, a.p)) + reach * angle(a, b);
        return travel > 1e-6 ? SKIN() / travel : 1;
    }
    template <class Backend>
    static bool sweepAll(const Frame& from, const Frame& to, const V* local, const double* radius, unsigned k,
                         Backend& backend, Result& out, double& first, double arc, double begin, double span)
    {
        first = 2;
        for (unsigned i = 0; i < k; ++i)
        {
            Contacts hits;
            if (out.queries >= MAX_QUERIES)
            {
                out.outcome = CAPACITY;
                return false;
            }
            ++out.queries;
            if (!backend.sweep(from.world(local[i]), to.world(local[i]), radius[i] + arc, hits, begin, span))
            {
                ++out.failures;
                out.outcome = QUERY_FAILED;
                return false;
            }
            if ((hits.block.present && !hits.block.usable()) || (hits.light.present && !hits.light.usable()))
            {
                ++out.failures;
                out.outcome = QUERY_FAILED;
                return false;
            }
            if (hits.light.present && begin <= 1)
            {
                Hit hit = hits.light;
                hit.fraction = begin + span * hit.fraction;
                if (!out.light || hit.fraction < out.lightHit.fraction)
                {
                    out.light = true;
                    out.lightHit = hit;
                }
            }
            if (hits.block.present && hits.block.fraction < first)
            {
                first = hits.block.fraction;
                out.block = hits.block;
                out.block.fraction = begin + span * first;
            }
        }
        return true;
    }
    template <class Backend>
    bool recoveryPath(const Frame& desired, const V* local, const double* radius, unsigned k, double reach,
                      Backend& backend, Result& out)
    {
        double endpoint = 2;
        if (!sweepAll(desired, desired, local, radius, k, backend, out, endpoint, 0, 2, 0) || endpoint <= 1)
            return false;
        const double theta = angle(prev_, desired),
                     stepAngle = reach > 1e-9 ? sqrt(8 * ARC_PAD() / reach) : .17453292519943295;
        const unsigned steps =
            (unsigned)ceil(theta / (stepAngle < .17453292519943295 ? stepAngle : .17453292519943295)) + 1;
        if (steps > MAX_STEPS)
        {
            out.outcome = CAPACITY;
            return false;
        }
        const double arc = reach * (theta / steps) * (theta / steps) / 8;
        for (unsigned i = 0; i < k; ++i)
        {
            bool leftOverlap = false;
            for (unsigned s = 0; s < steps; ++s)
            {
                const V a = between(prev_, desired, (double)s / steps).world(local[i]);
                const V b = between(prev_, desired, (double)(s + 1) / steps).world(local[i]);
                Contacts hit;
                if (out.queries >= MAX_QUERIES)
                {
                    out.outcome = CAPACITY;
                    return false;
                }
                ++out.queries;
                if (!backend.sweep(a, b, radius[i] + arc, hit, 2, 0))
                {
                    ++out.failures;
                    out.outcome = QUERY_FAILED;
                    return false;
                }
                if (!hit.block.present)
                {
                    leftOverlap = true;
                    continue;
                }
                if (!hit.block.usable())
                {
                    ++out.failures;
                    out.outcome = QUERY_FAILED;
                    return false;
                }
                if (leftOverlap || hit.block.fraction > 0 ||
                    gx_physics::dot(gx_physics::sub(b, a), hit.block.normal) < -.000001)
                    return false;
                // Reverse sweep to the original anchor: the first face must be the free
                // side we are retreating towards. No clear endpoint can waive this sweep.
                Contacts reverse;
                if (out.queries >= MAX_QUERIES)
                {
                    out.outcome = CAPACITY;
                    return false;
                }
                ++out.queries;
                if (!backend.sweep(desired.world(local[i]), a, radius[i] + arc, reverse, 2, 0))
                {
                    ++out.failures;
                    out.outcome = QUERY_FAILED;
                    return false;
                }
                if (!reverse.block.present || !reverse.block.usable() || reverse.block.fraction <= 0 ||
                    gx_physics::dot(gx_physics::sub(desired.world(local[i]), a), reverse.block.normal) <= 0)
                    return false;
            }
        }
        return true;
    }
    Result fallback(Result out, const Frame& desired)
    {
        out.valid = true;
        out.unavailable = true;
        out.blocked = anchored_ || out.blocked;
        out.solved = anchored_ ? prev_ : desired;
        out.light = false;
        return out;
    }

  public:
    // metres kept free in front of a blocking surface
    static double SKIN()
    {
        return .004;
    }
    // Typical adult arm reach 70 cm plus the hands' 10 cm margin. This fixed bound
    // limits root/holding-hand correction, not weapon length or ordinary wall blocking.
    static double ESCAPE_DISTANCE()
    {
        return .80;
    }
    static double JAM_SECONDS()
    {
        return .30;
    }
    Solver() : ready_(false), anchored_(false), object_(0), jam_(false), ghost_(false), jamTime_(0), away_(0)
    {
    }
    void reset()
    {
        ready_ = anchored_ = jam_ = ghost_ = false;
        object_ = 0;
        jamTime_ = away_ = 0;
    }
    bool ready() const
    {
        return ready_;
    }
    bool ghost() const
    {
        return ghost_;
    }
    const Frame& previous() const
    {
        return prev_;
    }
    // No native shape this frame: preserve the last pose, or anchor the first attempted one. This never claims clearance.
    Result holdUnavailable(uint64_t object, const Frame& desired)
    {
        Result out;
        if (!object || !desired.rigid())
        {
            reset();
            return out;
        }
        if (object != object_)
        {
            reset();
            object_ = object;
        }
        if (!anchored_)
        {
            prev_ = desired;
            anchored_ = true;
        }
        return fallback(out, desired);
    }
    // backend.sweep(from,to,r,Contacts&,begin,span): synchronous success, independent hit classes.
    // begin/span maps segment-local fractions to frame time; begin>1 means proof query (no impact reporting).
    template <class Backend>
    Result update(uint64_t object, const Frame& desired, const Capsule* caps, unsigned n, Backend& backend,
                  double dt = .011)
    {
        Result out;
        if (!object || !desired.rigid() || !caps || !n)
        {
            reset();
            return out;
        }
        if (object != object_)
        {
            reset();
            object_ = object;
        }
        const bool firstPose = !anchored_;
        if (firstPose)
        {
            prev_ = desired;
            anchored_ = true;
        }
        V local[MAX_POINTS];
        double radius[MAX_POINTS];
        const unsigned k = points(caps, n, local, radius, MAX_POINTS);
        if (!k)
        {
            out.outcome = CAPACITY;
            return fallback(out, desired);
        }
        double reach = 0;
        for (unsigned i = 0; i < k; ++i)
        {
            const double d = gx_physics::norm(local[i]);
            if (d > reach)
                reach = d;
        }
        if (jam_)
        {
            const double separation = gx_physics::norm(gx_physics::sub(desired.p, prev_.p));
            const double outward = gx_physics::dot(gx_physics::sub(desired.p, prev_.p), jamNormal_);
            if (outward > away_)
                away_ = outward;
            if (away_ > .002 && gx_physics::finite(dt) && dt > 0 && dt <= .1)
                jamTime_ += dt;
            // The elapsed-overlap clock is independent of the former 30 cm rule.
            if (jamTime_ >= JAM_SECONDS() && separation > ESCAPE_DISTANCE())
                ghost_ = true;
            if (recoveryPath(desired, local, radius, k, reach, backend, out))
            {
                prev_ = desired;
                ready_ = true;
                jam_ = ghost_ = false;
                jamTime_ = away_ = 0;
                out.valid = true;
                out.unavailable = false;
                out.blocked = out.light = false;
                out.solved = desired;
                out.outcome = RECOVERED;
                return out;
            }
            if (jamTime_ >= JAM_SECONDS() && away_ > .002)
            {
                const double length = gx_physics::norm(jamNormal_);
                if (length > .5)
                {
                    const double distances[] = {.05, .10, .20, .40, .80};
                    for (unsigned attempt = 0; attempt < sizeof(distances) / sizeof(distances[0]); ++attempt)
                    {
                        Frame candidate = desired;
                        candidate.p = gx_physics::add(
                            candidate.p, gx_physics::mul(jamNormal_, distances[attempt] / length));
                        if (recoveryPath(candidate, local, radius, k, reach, backend, out))
                        {
                            prev_ = candidate;
                            ready_ = true;
                            jam_ = ghost_ = false;
                            jamTime_ = away_ = 0;
                            out.valid = true;
                            out.unavailable = false;
                            out.blocked = true;
                            out.light = false;
                            out.solved = candidate;
                            out.outcome = RECOVERED;
                            return out;
                        }
                        if (out.outcome == QUERY_FAILED || out.outcome == CAPACITY)
                            break;
                    }
                }
            }
            if (ghost_)
            {
                out.valid = true;
                out.unavailable = true;
                out.blocked = out.light = false;
                out.solved = desired;
                out.outcome = ESCAPED;
                return out;
            }
            if (out.outcome == QUERY_FAILED || out.outcome == CAPACITY)
                return fallback(out, desired);
            out.outcome = INITIAL_OVERLAP;
            out.blocked = true;
            return fallback(out, desired);
        }
        double first = 2;
        // Unknown geometry/query loss retains the old 10 cm reseed gate. A confirmed
        // penetrating seed has its own swept depenetration route at ANY retreat distance.
        if (!ready_)
        {
            if (gx_physics::norm(gx_physics::sub(desired.p, prev_.p)) > .10 ||
                angle(prev_, desired) > .17453292519943295)
            {
                out.outcome = RECOVERY_LIMIT;
                return fallback(out, desired);
            }
            if (!sweepAll(desired, desired, local, radius, k, backend, out, first, 0, 2, 0))
                return fallback(out, desired);
            if (first <= 1)
            {
                jam_ = true;
                jamNormal_ = out.block.normal;
                out.outcome = INITIAL_OVERLAP;
                out.blocked = true;
                return fallback(out, desired);
            }
            prev_ = desired;
            ready_ = true;
            out.valid = true;
            out.unavailable = false;
            out.outcome = firstPose ? FREE : RECOVERED;
            out.solved = desired;
            return out;
        }
        const double theta = angle(prev_, desired),
                     maxAngle = reach > 1e-9 ? sqrt(8 * ARC_PAD() / reach) : .17453292519943295;
        const double stepAngle = maxAngle < .17453292519943295 ? maxAngle : .17453292519943295;
        const unsigned steps = (unsigned)ceil(theta / stepAngle) + 1;
        if (steps > MAX_STEPS)
        {
            out.outcome = CAPACITY;
            return fallback(out, desired);
        }
        // For constant-speed slerp, |point''(u)| <= reach*theta^2. The deviation from each segment chord is
        // <= reach*(theta/steps)^2/8. Inflating the spatial covering spheres by that amount covers the entire arc.
        const double arc = reach * (theta / steps) * (theta / steps) / 8;
        Frame segFrom = prev_, segTo = desired;
        unsigned segment = 0;
        for (unsigned s = 0; s < steps; ++s)
        {
            segFrom = between(prev_, desired, (double)s / steps);
            segTo = between(prev_, desired, (double)(s + 1) / steps);
            if (!sweepAll(segFrom, segTo, local, radius, k, backend, out, first, arc, (double)s / steps,
                          1.0 / steps))
                return fallback(out, desired);
            if (first <= 1)
            {
                segment = s;
                break;
            }
        }
        Frame solved = desired;
        if (first <= 1)
        {
            solved = between(segFrom, segTo, first - skinFraction(segFrom, segTo, reach));
            out.blocked = true;
            out.fraction = (segment + first) / steps;
            const Hit primary = out.block;
            // Slide (as the hands do): the remaining motion minus its component INTO the blocking surface, one more sweep.
            V normal = out.block.normal;
            const double nn = gx_physics::norm(normal);
            if (first > 0 && nn > .5)
            {
                normal = gx_physics::mul(normal, 1 / nn);
                V rest = gx_physics::sub(desired.p, solved.p);
                const double into = gx_physics::dot(rest, normal);
                if (into < 0)
                    rest = gx_physics::sub(rest, gx_physics::mul(normal, into));
                Frame slide = solved;
                slide.p = gx_physics::add(solved.p, rest);
                if (gx_physics::norm(rest) > 1e-5)
                {
                    double again = 2;
                    if (!sweepAll(solved, slide, local, radius, k, backend, out, again, 0, 2, 0))
                        return fallback(out, desired);
                    solved = again <= 1 ? between(solved, slide, again - skinFraction(solved, slide, reach))
                                        : slide;
                }
            }
            // A zero-time hit is an actual overlap, unlike an ordinary successful stop.
            // Keep the last solved anchor for every proof, including while ghosted.
            if (first <= 0)
            {
                jam_ = true;
                jamNormal_ = primary.normal;
                if (recoveryPath(desired, local, radius, k, reach, backend, out))
                {
                    solved = desired;
                    out.blocked = false;
                    out.outcome = RECOVERED;
                    out.light = false;
                    jam_ = false;
                    jamTime_ = away_ = 0;
                }
                else if (out.outcome == QUERY_FAILED || out.outcome == CAPACITY)
                    return fallback(out, desired);
            }
            out.block = primary;
        }
        if (!solved.rigid())
        {
            reset();
            return out;
        }
        // contact behind a wall is unreachable
        if (out.blocked && out.light && out.lightHit.fraction > out.fraction)
            out.light = false;
        prev_ = solved;
        out.valid = true;
        out.unavailable = false;
        out.solved = solved;
        if (out.outcome != RECOVERED)
            out.outcome = out.blocked ? BLOCKED : FREE;
        return out;
    }
};
}
