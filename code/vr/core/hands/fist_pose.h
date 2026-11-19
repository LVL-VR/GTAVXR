#pragma once
// Trigger latch and finger-rotation blending for the native GTA hand pose.
#include <math.h>
#include <float.h>

namespace gx_fist
{
enum
{
    FINGERS = 5, // thumb, index, middle, ring, little
    JOINTS = 3,  // root, middle, tip
    BONES = FINGERS * JOINTS
};

// Trigger travel that closes the fist and the lower one that opens it again.
const double ENTER_TRIGGER = .65, EXIT_TRIGGER = .45;
const double CLOSE_SECONDS = .08, OPEN_SECONDS = .10;

struct V
{
    double x, y, z;
    V(double a = 0, double b = 0, double c = 0) : x(a), y(b), z(c) {}
};
inline double dot(V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline bool finite(V v) { return _finite(v.x) && _finite(v.y) && _finite(v.z); }
// The three unit axes of one local joint rotation.
struct Basis
{
    V a, b, c;
};
inline bool orthonormal(const Basis& m)
{
    return finite(m.a) && finite(m.b) && finite(m.c) && fabs(dot(m.a, m.a) - 1) < 1e-3 &&
           fabs(dot(m.b, m.b) - 1) < 1e-3 && fabs(dot(m.c, m.c) - 1) < 1e-3 && fabs(dot(m.a, m.b)) < 1e-3 &&
           fabs(dot(m.a, m.c)) < 1e-3 && fabs(dot(m.b, m.c)) < 1e-3;
}
// An incomplete or malformed native clip cannot supply a pose for this hand.
template<class Sample>
bool sampleHand(float (&rotations)[BONES][4], const Sample& sample)
{
    for (unsigned joint = 0; joint < BONES; ++joint)
    {
        float* q = rotations[joint];
        if (!sample(joint, q))
            return false;
        double squaredLength = 0;
        for (unsigned axis = 0; axis < 4; ++axis)
            squaredLength += double(q[axis]) * double(q[axis]);
        if (!_finite(squaredLength) || squaredLength < .98 || squaredLength > 1.02)
            return false;
        const float scale = (float)(1.0 / sqrt(squaredLength));
        for (unsigned axis = 0; axis < 4; ++axis)
            q[axis] *= scale;
    }
    return true;
}

// Unit quaternion (x, y, z, w) of a basis and back; a matching pair used only to blend.
inline void quaternion(const Basis& m, double* q)
{
    const double trace = m.a.x + m.b.y + m.c.z;
    if (trace > 0)
    {
        const double s = sqrt(trace + 1) * 2;
        q[3] = s / 4;
        q[0] = (m.b.z - m.c.y) / s;
        q[1] = (m.c.x - m.a.z) / s;
        q[2] = (m.a.y - m.b.x) / s;
    }
    else if (m.a.x > m.b.y && m.a.x > m.c.z)
    {
        const double s = sqrt(1 + m.a.x - m.b.y - m.c.z) * 2;
        q[3] = (m.b.z - m.c.y) / s;
        q[0] = s / 4;
        q[1] = (m.b.x + m.a.y) / s;
        q[2] = (m.c.x + m.a.z) / s;
    }
    else if (m.b.y > m.c.z)
    {
        const double s = sqrt(1 + m.b.y - m.a.x - m.c.z) * 2;
        q[3] = (m.c.x - m.a.z) / s;
        q[0] = (m.b.x + m.a.y) / s;
        q[1] = s / 4;
        q[2] = (m.c.y + m.b.z) / s;
    }
    else
    {
        const double s = sqrt(1 + m.c.z - m.a.x - m.b.y) * 2;
        q[3] = (m.a.y - m.b.x) / s;
        q[0] = (m.c.x + m.a.z) / s;
        q[1] = (m.c.y + m.b.z) / s;
        q[2] = s / 4;
    }
}
inline Basis basis(const double* q)
{
    const double x = q[0], y = q[1], z = q[2], w = q[3];
    Basis m;
    m.a = V(1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w));
    m.b = V(2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w));
    m.c = V(2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y));
    return m;
}
// amount 0 returns the animated joint, 1 the closed one, along the shortest rotation.
inline bool blend(const Basis& animated, const Basis& fist, double amount, Basis& out)
{
    if (!orthonormal(animated) || !orthonormal(fist) || !_finite(amount))
        return false;
    if (amount >= 1)
    {
        out = fist;
        return true;
    }
    if (amount <= 0)
    {
        out = animated;
        return true;
    }
    double a[4], b[4], q[4];
    quaternion(animated, a);
    quaternion(fist, b);
    double cosine = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if (cosine < 0)
    {
        cosine = -cosine;
        for (unsigned i = 0; i < 4; ++i)
            b[i] = -b[i];
    }
    double wa = 1 - amount, wb = amount;
    if (cosine < .9995)
    {
        const double angle = acos(cosine), sine = sin(angle);
        wa = sin((1 - amount) * angle) / sine;
        wb = sin(amount * angle) / sine;
    }
    double norm = 0;
    for (unsigned i = 0; i < 4; ++i)
    {
        q[i] = wa * a[i] + wb * b[i];
        norm += q[i] * q[i];
    }
    if (!_finite(norm) || norm < 1e-12)
        return false;
    norm = 1 / sqrt(norm);
    for (unsigned i = 0; i < 4; ++i)
        q[i] *= norm;
    out = basis(q);
    return orthonormal(out);
}

// Per-hand entry and exit. amount 0 means "do not write": the animated pose owns the fingers.
class Latch
{
    bool closed_;
    double amount_, last_;

  public:
    Latch() : closed_(false), amount_(0), last_(-1) {}
    void reset()
    {
        closed_ = false;
        amount_ = 0;
        last_ = -1;
    }
    bool closed() const { return closed_; }
    // free: the hand is tracked, empty and allowed to show the pose this update. Losing that returns
    // the fingers at once, because a weapon, object, phone or wheel now owns them. Releasing the
    // trigger opens the hand over OPEN_SECONDS. Repeated calls with the same time return the same value.
    double update(double seconds, double trigger, bool free)
    {
        if (!_finite(seconds) || !free || !_finite(trigger) || (last_ >= 0 && seconds < last_))
        {
            reset();
            if (_finite(seconds) && free)
                last_ = seconds;
            return 0;
        }
        closed_ = trigger >= (closed_ ? EXIT_TRIGGER : ENTER_TRIGGER) && trigger <= 1.0001;
        double elapsed = last_ >= 0 ? seconds - last_ : 0;
        if (elapsed > .25)
            elapsed = .25;
        last_ = seconds;
        amount_ += closed_ ? elapsed / CLOSE_SECONDS : -elapsed / OPEN_SECONDS;
        if (amount_ < 0)
            amount_ = 0;
        if (amount_ > 1)
            amount_ = 1;
        return amount_;
    }
};

// What the hand must be for the fist to show. Each field is one owner or condition that outranks it.
struct Situation
{
    // freeHand comes from the arm writer: the tracked pose was applied and no weapon, attached prop,
    // phone, steering control or native reload owns that hand this update.
    bool active, localHuman, tracked, freeHand, unarmed, heldObject, phone, wheel, menu, holster, paused,
        cinematic, driver, injured;
    Situation()
        : active(false), localHuman(false), tracked(false), freeHand(false), unarmed(false), heldObject(false),
          phone(false), wheel(false), menu(false), holster(false), paused(false), cinematic(false),
          driver(false), injured(false)
    {
    }
};
inline bool admitted(const Situation& s)
{
    return s.active && s.localHuman && s.tracked && s.freeHand && s.unarmed && !s.heldObject && !s.phone &&
           !s.wheel && !s.menu && !s.holster && !s.paused && !s.cinematic && !s.driver && !s.injured;
}
}
