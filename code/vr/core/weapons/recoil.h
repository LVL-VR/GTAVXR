#pragma once
 
 
 
 
// final arm/weapon pose -> today's "Disminuido". REALISTIC = one kick per ACCEPTED shot (CommitWeaponHandFire), applied to the
// held weapon AND its hand as one rigid motion about the grip (muzzle up + back), returning with a critically damped spring.
 
 
#include <math.h>
namespace gx_recoil
{
enum Group
{
    G_PISTOL,
    G_SMG,
    G_RIFLE,
    G_MG,
    G_SHOTGUN,
    G_SNIPER,
    G_HEAVY,
    G_OTHER,
    G_COUNT
};
inline Group group(unsigned groupHash)
{
    switch (groupHash)
    {
    case 0x18D5FA97u:
        return G_PISTOL;
    case 0xC6E9A5C5u:
        return G_SMG;
    case 0x39D5C192u:
        return G_RIFLE;
    case 0x451B04BCu:
        return G_MG;
    case 0x33431399u:
        return G_SHOTGUN;
    case 0xB7BBD827u:
        return G_SNIPER;
    case 0xA27A4F9Fu:
        return G_HEAVY;
    default:
        return G_OTHER;
    }
}
struct Kick
{
    double pitch, back;
    Kick() : pitch(0), back(0) {}
}; // peak radians / metres of ONE isolated shot
 
// carbine .333, MG 1.0, pump shotgun 2.0, sniper .28, heavy 1.0). Variants scale by their own amplitude, clamped 0.5..2.
// Reference amplitude is supplied from the host weapon data.
double referenceAmplitude(Group group);
inline Kick kick(Group g, double shakeAmplitude, double ikDisplacement)
{
    static const double baseDeg[G_COUNT] = {7.0, 3.0, 4.0, 3.5, 11.0, 8.0, 5.0, 3.0},
                        baseM[G_COUNT] = {.018, .010, .015, .015, .040, .030, .030, .010};
    Kick out;
    if ((unsigned)g >= (unsigned)G_COUNT)
        return out;
    const double reference = referenceAmplitude(g);
    double scale = (shakeAmplitude > 0 && reference > 0) ? shakeAmplitude / reference : 1.0;
    if (!(scale == scale))
        scale = 1.0;
    if (scale < .5)
        scale = .5;
    if (scale > 2)
        scale = 2;
    out.pitch = baseDeg[g] * scale * 3.14159265358979323846 / 180.0;
    out.back = baseM[g] * scale + ((ikDisplacement > 0 && ikDisplacement < .1) ? ikDisplacement : 0);
    return out;
}
// Critically damped return, omega = 2*pi*3 Hz: an isolated kick peaks after 1/omega (~53 ms) and is ~95% recovered by ~0.25 s.
// Sustained automatic fire accumulates but is clamped (MAX_PITCH / MAX_BACK) so the weapon never leaves the hand.
const double OMEGA = 18.84955592153876, MAX_PITCH = .35, MAX_BACK = .08;
class State
{
    double p_, pv_, b_, bv_, t_;
    bool live_;
    static void step(double& x, double& v, double dt)
    { // exact critically damped solution over dt
        const double e = exp(-OMEGA * dt), x0 = x, v0 = v;
        x = (x0 + (v0 + OMEGA * x0) * dt) * e;
        v = (v0 - (v0 + OMEGA * x0) * OMEGA * dt) * e;
    }

  public:
    State() : p_(0), pv_(0), b_(0), bv_(0), t_(0), live_(false) {}
    void reset()
    {
        *this = State();
    }
    bool live() const
    {
        return live_;
    }
    void advance(double now)
    {
        if (!live_)
            return;
        const double dt = now - t_;
        if (!(dt == dt) || dt < 0 || dt > 1.0)
        {
            reset();
            return;
        }
        step(p_, pv_, dt);
        step(b_, bv_, dt);
        t_ = now;
        if (fabs(p_) < 1e-5 && fabs(pv_) < 1e-4 && fabs(b_) < 1e-6 && fabs(bv_) < 1e-5)
            reset(); // settled: exact identity again
    }
    // Impulse giving an isolated peak of k: x(t)=v0*t*exp(-OMEGA t) peaks at v0/(OMEGA*e).
    void fire(const Kick& k, bool supported, double now)
    {
        if (!(now == now))
            return;
        advance(now);
        if (!live_)
        {
            t_ = now;
            live_ = true;
        }
        const double gain = OMEGA * 2.718281828459045, pitch = supported ? .5 * k.pitch : k.pitch;
        pv_ += pitch * gain;
        bv_ += k.back * gain;
    }
    double pitch() const
    {
        const double v = p_ < 0 ? 0 : p_;
        return v > MAX_PITCH ? MAX_PITCH : v;
    }
    double back() const
    {
        const double v = b_ < 0 ? 0 : b_;
        return v > MAX_BACK ? MAX_BACK : v;
    }
};
 
// c by `pitch` (axis k = a x c, so k x a = c for either handedness), then the frame moves back along the ORIGINAL a by `back`.
// The same call is applied to the weapon and to its hand so they stay rigidly attached. Returns false on a degenerate frame.
struct Frame
{
    double a[3], b[3], c[3], d[3];
};
inline void rotate(const double* k, double cs, double sn, const double* v, double* out)
{
    const double kv = k[0] * v[0] + k[1] * v[1] + k[2] * v[2],
                 x[3] = {k[1] * v[2] - k[2] * v[1], k[2] * v[0] - k[0] * v[2], k[0] * v[1] - k[1] * v[0]};
    for (unsigned i = 0; i < 3; ++i)
        out[i] = v[i] * cs + x[i] * sn + k[i] * kv * (1 - cs);
}
inline bool kickFrame(const double* gunA, const double* gunC, const double* pivot, double pitch, double back,
                      Frame& f)
{
    double k[3] = {gunA[1] * gunC[2] - gunA[2] * gunC[1], gunA[2] * gunC[0] - gunA[0] * gunC[2],
                   gunA[0] * gunC[1] - gunA[1] * gunC[0]};
    const double n = sqrt(k[0] * k[0] + k[1] * k[1] + k[2] * k[2]),
                 na = sqrt(gunA[0] * gunA[0] + gunA[1] * gunA[1] + gunA[2] * gunA[2]);
    if (!(n > 1e-4) || !(na > 1e-4) || !(pitch == pitch) || !(back == back))
        return false;
    for (unsigned i = 0; i < 3; ++i)
        k[i] /= n;
    const double cs = cos(pitch), sn = sin(pitch);
    double t[3], r[3], rel[3];
    rotate(k, cs, sn, f.a, t);
    for (unsigned i = 0; i < 3; ++i)
        f.a[i] = t[i];
    rotate(k, cs, sn, f.b, t);
    for (unsigned i = 0; i < 3; ++i)
        f.b[i] = t[i];
    rotate(k, cs, sn, f.c, t);
    for (unsigned i = 0; i < 3; ++i)
        f.c[i] = t[i];
    for (unsigned i = 0; i < 3; ++i)
        rel[i] = f.d[i] - pivot[i];
    rotate(k, cs, sn, rel, r);
    for (unsigned i = 0; i < 3; ++i)
        f.d[i] = pivot[i] + r[i] - gunA[i] / na * back;
    return true;
}
}
