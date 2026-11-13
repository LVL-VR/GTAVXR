#pragma once
 
 
 
// WRAP (pistols, revolvers): the support hand closes around the gun hand; aim stays from the gun hand, steadied.
// FRONT (one-handed SMGs / machine pistols): the support hand on the magazine or the front, solved like a rifle foregrip.
// Prior art (H3VR / Bonelab / Pavlov): a pistol two-hand hold steadies the gun, it does not re-aim it from the support hand.
#include <math.h>
namespace gx_one_hand
{
enum Kind
{
    NONE = 0,
    WRAP = 1,
    FRONT = 2
};
const unsigned GROUP_PISTOL = 0x18D5FA97u, GROUP_SMG = 0xC6E9A5C5u;  
const double ACQUIRE_M =
    .15; // the support trigger takes the gun only with the hand this close to the support point
const double FRONT_SLIP_M =
    .06; // a FRONT hold keeps the hand where it closed, at most this far from the point
const double MIN_FRONT_M =
    .04; // the magazine is a front point only this far ahead of the grip (TEC-9 yes, Uzi no)
const double STEADY_S = .06; // WRAP: time constant of the steadied gun rotation
inline Kind kind(bool twoHanded, bool gun, unsigned group, bool on)
{
    if (!on || twoHanded || !gun)
        return NONE;
    if (group == GROUP_PISTOL)
        return WRAP;
    if (group == GROUP_SMG)
        return FRONT;
    return NONE;
}
inline double dot3(const double* a, const double* b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
inline double dist3(const double* a, const double* b)
{
    const double d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
    return sqrt(dot3(d, d));
}
// FRONT support point in weapon space: the magazine when it sits ahead of the grip along the barrel, else a point on the
// grip -> muzzle line (55 %, at most 12 cm ahead of the grip). false when the bones give no barrel.
inline bool frontPoint(const double* grip, const double* muzzle, const double* clip, double* out)
{
    double axis[3] = {muzzle[0] - grip[0], muzzle[1] - grip[1], muzzle[2] - grip[2]};
    const double length = sqrt(dot3(axis, axis));
    if (!(length > .02) || length > 2.)
        return false;
    for (unsigned c = 0; c < 3; ++c)
        axis[c] /= length;
    if (clip)
    {
        const double ahead[3] = {clip[0] - grip[0], clip[1] - grip[1], clip[2] - grip[2]};
        if (dot3(ahead, axis) >= MIN_FRONT_M)
        {
            for (unsigned c = 0; c < 3; ++c)
                out[c] = clip[c];
            return true;
        }
    }
    const double along = length * .55 < .12 ? length * .55 : .12;
    for (unsigned c = 0; c < 3; ++c)
        out[c] = grip[c] + axis[c] * along;
    return true;
}
// The captured hand position kept within `slip` of the point (FRONT) - the hand stays where it closed, never drifts away.
inline void clampNear(const double* point, double slip, double* inOut)
{
    const double d = dist3(inOut, point);
    if (d <= slip || !(d > 0))
        return;
    for (unsigned c = 0; c < 3; ++c)
        inOut[c] = point[c] + (inOut[c] - point[c]) * (slip / d);
}
// ---- WRAP steady rotation: rotation rows a, b, c (orthonormal, 9 doubles) <-> unit quaternion, normalized lerp blend
struct Q
{
    double x, y, z, w;
};
inline Q fromRows(const double* m)
{
    // m = a(0..2) b(3..5) c(6..8); same convention both ways, so the blend is consistent whatever the basis meaning
    const double t = m[0] + m[4] + m[8];
    Q q;
    if (t > 0)
    {
        const double s = sqrt(t + 1.) * 2;
        q.w = .25 * s;
        q.x = (m[5] - m[7]) / s;
        q.y = (m[6] - m[2]) / s;
        q.z = (m[1] - m[3]) / s;
    }
    else if (m[0] > m[4] && m[0] > m[8])
    {
        const double s = sqrt(1. + m[0] - m[4] - m[8]) * 2;
        q.w = (m[5] - m[7]) / s;
        q.x = .25 * s;
        q.y = (m[3] + m[1]) / s;
        q.z = (m[6] + m[2]) / s;
    }
    else if (m[4] > m[8])
    {
        const double s = sqrt(1. + m[4] - m[0] - m[8]) * 2;
        q.w = (m[6] - m[2]) / s;
        q.x = (m[3] + m[1]) / s;
        q.y = .25 * s;
        q.z = (m[7] + m[5]) / s;
    }
    else
    {
        const double s = sqrt(1. + m[8] - m[0] - m[4]) * 2;
        q.w = (m[1] - m[3]) / s;
        q.x = (m[6] + m[2]) / s;
        q.y = (m[7] + m[5]) / s;
        q.z = .25 * s;
    }
    return q;
}
inline void toRows(const Q& q, double* m)
{
    const double x = q.x, y = q.y, z = q.z, w = q.w;
    m[0] = 1 - 2 * (y * y + z * z);
    m[1] = 2 * (x * y + z * w);
    m[2] = 2 * (x * z - y * w);
    m[3] = 2 * (x * y - z * w);
    m[4] = 1 - 2 * (x * x + z * z);
    m[5] = 2 * (y * z + x * w);
    m[6] = 2 * (x * z + y * w);
    m[7] = 2 * (y * z - x * w);
    m[8] = 1 - 2 * (x * x + y * y);
}
struct Steady
{
    bool valid;
    Q q;
    double at;
    Steady() : valid(false), at(0)
    {
        q.x = q.y = q.z = 0;
        q.w = 1;
    }
    void reset()
    {
        valid = false;
    }
    // rows: the gun rotation this frame (replaced by the steadied one). A gap longer than .2 s restarts from the input.
    void apply(double* rows, double now)
    {
        const Q in = fromRows(rows);
        if (!valid || now - at > .2 || now < at)
        {
            q = in;
            valid = true;
            at = now;
            return;
        }
        const double dt = now - at;
        at = now;
        const double k = 1. - exp(-dt / STEADY_S);
        Q t = in;
        if (q.x * t.x + q.y * t.y + q.z * t.z + q.w * t.w < 0)
        {
            t.x = -t.x;
            t.y = -t.y;
            t.z = -t.z;
            t.w = -t.w;
        }
        Q r = {q.x + (t.x - q.x) * k, q.y + (t.y - q.y) * k, q.z + (t.z - q.z) * k, q.w + (t.w - q.w) * k};
        const double n = sqrt(r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w);
        if (!(n > 1e-9))
        {
            q = in;
            return;
        }
        r.x /= n;
        r.y /= n;
        r.z /= n;
        r.w /= n;
        q = r;
        toRows(q, rows);
    }
};
// A hold: taken on the trigger press only when near; held until the trigger is released (< .2) or the gun changes.
struct Hold
{
    unsigned long long object;
    bool active, refused;
    double at;
    double hand
        [12]; // the support grip frame in weapon space at the moment it closed (rows a b c, then position)
    Hold() : object(0), active(false), refused(false), at(0)
    {
        for (unsigned i = 0; i < 12; ++i)
            hand[i] = 0;
    }
    void reset()
    {
        object = 0;
        active = refused = false;
    }
    // returns true while held; `capture` is the current support grip frame in weapon space, `point` the support point.
    // Not asked for .1 s (the trigger was released and the caller stopped asking) = released: a new press needs the point.
    bool update(unsigned long long weapon, float trigger, const double* capture, const double* point,
                double now)
    {
        if (now - at > .1 || now < at)
            reset();
        at = now;
        if (trigger < .2f || !weapon)
        {
            reset();
            return false;
        }
        if (object != weapon)
        {
            reset();
            object = weapon;
        }
        if (active)
            return true;
        if (refused)
            return false;
        if (dist3(capture + 9, point) > ACQUIRE_M)
        {
            refused = true;
            return false;
        }
        for (unsigned i = 0; i < 12; ++i)
            hand[i] = capture[i];
        active = true;
        return true;
    }
};
} // namespace gx_one_hand
