#pragma once
#include "../core/vector_math.h"
 
 
// Row-vector 4x4 like the bridge (reference084): rows 0..2 = right, up, back; row 3 = position; world = local * base.
// The bridge levels whatever anchor it gets (forward = -row2 projected, rows r, z, -f). The EXE gives it `level` (a stable
// leveled anchor that never points near vertical) and right-multiplies every returned world pose (eye cameras, centre,
// hand / aim targets) by `delta` = level^-1 * target, a rotation about the seat eye:
//   LEVEL        target = level (delta = identity: today's view, heading hardened)
 
 
// SMOOTHED follow low-passes the attitude in the level frame (turning is never delayed, only pitch / roll changes).
#include <cmath>
#include <float.h>

namespace gx_vehicle_camera
{

enum Mode
{
    LEVEL = 0,
    FULL_TILT = 1,
    HORIZON_LOCK = 2,
    MODE_COUNT = 3
};
enum Follow
{
    DIRECT = 0,
    SMOOTHED = 1,
    FOLLOW_COUNT = 2
};
enum Type
{
    CAR = 0,
    MOTORCYCLE = 1,
    PLANE = 2,
    HELICOPTER = 3,
    BOAT = 4,
    TYPE_COUNT = 5
};

 
// follow DIRECT everywhere.
inline int defaultMode(unsigned type)
{
    static const int modes[TYPE_COUNT] = {LEVEL, HORIZON_LOCK, FULL_TILT, FULL_TILT, LEVEL};
    return type < TYPE_COUNT ? modes[type] : LEVEL;
}
inline int sanitizeMode(int m, unsigned type)
{
    return m >= 0 && m < MODE_COUNT ? m : defaultMode(type);
}
inline int sanitizeFollow(int f)
{
    return f == SMOOTHED ? SMOOTHED : DIRECT;
}
inline const char* typeKey(unsigned type)
{
    static const char* keys[TYPE_COUNT] = {"vehicle_camera_car", "vehicle_camera_motorcycle",
                                           "vehicle_camera_plane", "vehicle_camera_helicopter",
                                           "vehicle_camera_boat"};
    return type < TYPE_COUNT ? keys[type] : "";
}
inline const char* followKey(unsigned type)
{
    static const char* keys[TYPE_COUNT] = {"vehicle_follow_car", "vehicle_follow_motorcycle",
                                           "vehicle_follow_plane", "vehicle_follow_helicopter",
                                           "vehicle_follow_boat"};
    return type < TYPE_COUNT ? keys[type] : "";
}

struct V
{
    double x, y, z;
    V() : x(0), y(0), z(0) {}
    V(double a, double b, double c) : x(a), y(b), z(c) {}
};
inline V add(V a, V b)
{
    return V(a.x + b.x, a.y + b.y, a.z + b.z);
}
inline V sub(V a, V b)
{
    return V(a.x - b.x, a.y - b.y, a.z - b.z);
}
inline V mul(V a, double s)
{
    return V(a.x * s, a.y * s, a.z * s);
}
inline double dot(V a, V b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline V cross(V a, V b)
{
    return gx_math::crossProduct(a, b);
}
inline double len(V a)
{
    return std::sqrt(dot(a, a));
}
inline bool finite(V a)
{
    return _finite(a.x) && _finite(a.y) && _finite(a.z);
}
inline V row(const double* m, unsigned r)
{
    return V(m[4 * r], m[4 * r + 1], m[4 * r + 2]);
}
inline void setRow(double* m, unsigned r, V v)
{
    m[4 * r] = v.x;
    m[4 * r + 1] = v.y;
    m[4 * r + 2] = v.z;
    m[4 * r + 3] = 0;
}

inline void identity(double* m)
{
    for (unsigned i = 0; i < 16; ++i)
        m[i] = (i % 5 == 0) ? 1 : 0;
}
inline void mul44(const double* a, const double* b, double* out)
{ // out = a * b (row vectors)
    double t[16];
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 4; ++j)
        {
            double s = 0;
            for (unsigned k = 0; k < 4; ++k)
                s += a[4 * i + k] * b[4 * k + j];
            t[4 * i + j] = s;
        }
    for (unsigned i = 0; i < 16; ++i)
        out[i] = t[i];
}
inline void rigidInverse(const double* m, double* out)
{ // inverse of a rigid row-vector transform
    double t[16];
    identity(t);
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 3; ++j)
            t[4 * i + j] = m[4 * j + i];
    for (unsigned j = 0; j < 3; ++j)
        t[12 + j] = -(m[12] * t[j] + m[13] * t[4 + j] + m[14] * t[8 + j]);
    for (unsigned i = 0; i < 16; ++i)
        out[i] = t[i];
}

// Stable level heading (item 9). Follows the horizontal direction of travel exactly as reference084::level does, except:
// - nose within ~75 deg of vertical (|horizontal forward| < 0.25): the previous heading is kept (no 180 deg flip at the
//   top of a loop, never a degenerate / near-vertical forward);
// - after passing the vertical the travel direction points the other way: while the vehicle is still upside down the view
//   keeps the previous side; once it is upright again it turns to the travel direction at kRecoverRate (no instant flip).
class Heading
{
    V h_;
    bool valid_, recovering_;

  public:
    static double kSteep()
    {
        return .25;
    }
    static double kRecoverRate()
    {
        return 3.14159265358979323846;
    } // rad/s: 180 deg in 1 s
    Heading() : valid_(false), recovering_(false) {}
    void reset()
    {
        valid_ = recovering_ = false;
    }
    bool valid() const
    {
        return valid_;
    }
    V get() const
    {
        return h_;
    }
    // forward = vehicle forward (-row2 of the full anchor), up = vehicle up (row1); dt in seconds (>= 0). harden = false
    // (item 9 switch OFF): exactly reference084::level's heading, refused when the forward is (near) vertical.
    bool update(V forward, V up, double dt, bool harden = true)
    {
        if (!finite(forward) || !finite(up) || !_finite(dt))
            return harden && valid_;
        if (dt < 0)
            dt = 0;
        V fh(forward.x, forward.y, 0);
        const double n = len(fh);
        if (!harden)
        {
            recovering_ = false;
            if (!(n > 1e-6))
            {
                valid_ = false;
                return false;
            }
            h_ = mul(fh, 1 / n);
            valid_ = true;
            return true;
        }
        if (!valid_)
        {
            if (n < 1e-3)
                return false; // first sample must have a heading
            h_ = mul(fh, 1 / n);
            valid_ = true;
            recovering_ = false;
            return true;
        }
        if (n < kSteep())
            return true; // steep: keep
        V target = mul(fh, 1 / n);
        if (dot(target, h_) < 0 && up.z < .2)
        {
            target = mul(target, -1);
            recovering_ = false;
        } // inverted: keep the side
        else if (dot(target, h_) < 0 || recovering_)
            recovering_ = true;
        if (!recovering_)
        {
            h_ = target;
            return true;
        }
        // upright again with the travel direction behind or beside: turn toward it at a bounded rate
        const double c = dot(h_, target), s = cross(h_, target).z;
        const double angle = std::atan2(s, c), step = kRecoverRate() * dt;
        if (std::fabs(angle) <= step || std::fabs(angle) < .02)
        {
            h_ = target;
            recovering_ = false;
            return true;
        }
        const double a = angle > 0 ? step : -step, ca = std::cos(a), sa = std::sin(a);
        h_ = V(h_.x * ca - h_.y * sa, h_.x * sa + h_.y * ca, 0);
        return true;
    }
};

// Basis builders (rows r, u, back; row 3 = p).
inline void levelBasis(V heading, V p, double* out)
{
    const V z(0, 0, 1), f = heading, r = cross(f, z);
    identity(out);
    setRow(out, 0, r);
    setRow(out, 1, z);
    setRow(out, 2, mul(f, -1));
    out[12] = p.x;
    out[13] = p.y;
    out[14] = p.z;
}
inline bool horizonBasis(V forward, V heading, V p, double* out)
{
    const V z(0, 0, 1);
    V f = forward;
    const double n = len(f);
    if (!(n > 1e-6))
        return false;
    f = mul(f, 1 / n);
    V r = cross(f, z);
    double rn = len(r);
     
    // right axis comes from the kept level heading, so the view does not jump at 60 deg.
    if (rn <= .5)
    {
        r = cross(heading, z);
        rn = len(r);
    }
    if (!(rn > 1e-6))
        return false;
    r = mul(r, 1 / rn);
    V u = cross(r, f);
    const double un = len(u);
    if (!(un > 1e-6))
        return false;
    u = mul(u, 1 / un);
    f = cross(u, r); // re-orthogonalize: back = -f, right x up = back
    identity(out);
    setRow(out, 0, r);
    setRow(out, 1, u);
    setRow(out, 2, mul(f, -1));
    out[12] = p.x;
    out[13] = p.y;
    out[14] = p.z;
    return true;
}

// Quaternion helpers for the SMOOTHED follow (attitude in the level frame).
struct Q
{
    double w, x, y, z;
    Q() : w(1), x(0), y(0), z(0) {}
    Q(double a, double b, double c, double d) : w(a), x(b), y(c), z(d) {}
};
inline Q fromRows(const double* m)
{ // rotation part of a row-vector matrix (rows are the images of the axes)
    const double m00 = m[0], m01 = m[1], m02 = m[2], m10 = m[4], m11 = m[5], m12 = m[6], m20 = m[8],
                 m21 = m[9], m22 = m[10];
    const double tr = m00 + m11 + m22;
    Q q;
    if (tr > 0)
    {
        const double s = std::sqrt(tr + 1) * 2;
        q = Q(s / 4, (m12 - m21) / s, (m20 - m02) / s, (m01 - m10) / s);
    }
    else if (m00 > m11 && m00 > m22)
    {
        const double s = std::sqrt(1 + m00 - m11 - m22) * 2;
        q = Q((m12 - m21) / s, s / 4, (m01 + m10) / s, (m20 + m02) / s);
    }
    else if (m11 > m22)
    {
        const double s = std::sqrt(1 + m11 - m00 - m22) * 2;
        q = Q((m20 - m02) / s, (m01 + m10) / s, s / 4, (m12 + m21) / s);
    }
    else
    {
        const double s = std::sqrt(1 + m22 - m00 - m11) * 2;
        q = Q((m01 - m10) / s, (m20 + m02) / s, (m12 + m21) / s, s / 4);
    }
    const double n = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    return Q(q.w / n, q.x / n, q.y / n, q.z / n);
}
inline void toRows(Q q, double* m)
{ // inverse of fromRows, rotation part only
    const double w = q.w, x = q.x, y = q.y, z = q.z;
    m[0] = 1 - 2 * (y * y + z * z);
    m[1] = 2 * (x * y + w * z);
    m[2] = 2 * (x * z - w * y);
    m[4] = 2 * (x * y - w * z);
    m[5] = 1 - 2 * (x * x + z * z);
    m[6] = 2 * (y * z + w * x);
    m[8] = 2 * (x * z + w * y);
    m[9] = 2 * (y * z - w * x);
    m[10] = 1 - 2 * (x * x + y * y);
}
inline Q slerp(Q a, Q b, double t)
{
    double c = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    if (c < 0)
    {
        b = Q(-b.w, -b.x, -b.y, -b.z);
        c = -c;
    }
    double ka = 1 - t, kb = t;
    if (c < .9995)
    {
        const double o = std::acos(c), s = std::sin(o);
        ka = std::sin((1 - t) * o) / s;
        kb = std::sin(t * o) / s;
    }
    Q q(a.w * ka + b.w * kb, a.x * ka + b.x * kb, a.y * ka + b.y * kb, a.z * ka + b.z * kb);
    const double n = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    return Q(q.w / n, q.x / n, q.y / n, q.z / n);
}

// One vehicle camera per seat session. update() takes the full anchor (as AnatomicalEyeAnchor builds it) and returns the
// leveled anchor for the bridge in `level` and the post-rotation in `delta` (and `full` = level^-1 * anchor, for item 8).
class Camera
{
    Heading heading_;
    Q smooth_;
    bool smoothValid_;

  public:
    static double kSmoothSeconds()
    {
        return .35;
    }
    Camera() : smoothValid_(false) {}
    void reset()
    {
        heading_.reset();
        smoothValid_ = false;
    }
    bool update(const double* anchor, int mode, int follow, double dt, double* level, double* delta,
                double* full, bool harden = true)
    {
        const V r = row(anchor, 0), u = row(anchor, 1), b = row(anchor, 2), p = row(anchor, 3),
                f = mul(b, -1);
        if (!finite(r) || !finite(u) || !finite(b) || !finite(p))
            return false;
        if (!heading_.update(f, u, dt, harden))
            return false;
        levelBasis(heading_.get(), p, level);
        double inv[16];
        rigidInverse(level, inv);
        mul44(inv, anchor, full);
        double target[16];
        if (mode == FULL_TILT)
            for (unsigned i = 0; i < 16; ++i)
                target[i] = anchor[i];
        else if (mode == HORIZON_LOCK)
        {
            if (!horizonBasis(f, heading_.get(), p, target))
                return false;
        }
        else
            for (unsigned i = 0; i < 16; ++i)
                target[i] = level[i];
        // attitude of the target in the level frame: target = local * level -> local = target * level^-1
        double local[16];
        mul44(target, inv, local);
        Q want = fromRows(local);
        if (follow == SMOOTHED && mode != LEVEL)
        {
            const double k = smoothValid_ ? 1 - std::exp(-(dt > 0 ? dt : 0) / kSmoothSeconds()) : 1;
            smooth_ = smoothValid_ ? slerp(smooth_, want, k) : want;
            smoothValid_ = true;
            want = smooth_;
        }
        else
            smoothValid_ = false;
        double rot[16];
        identity(rot);
        toRows(want, rot);
        double t2[16];
        mul44(rot, level, t2); // smoothed target = local * level
        mul44(inv, t2, delta); // delta = level^-1 * target (rotation about the seat eye)
        return true;
    }
};

// Item 8: a hand already mapped with `delta` (shown pose) re-expressed in the vehicle-attitude frame, so a still hand on a
// control reads no input when the vehicle banks / pitches: hand_control = hand_shown * delta^-1 * full.
inline void controlCorrection(const double* delta, const double* full, double* out)
{
    double inv[16];
    rigidInverse(delta, inv);
    mul44(inv, full, out);
}

}
