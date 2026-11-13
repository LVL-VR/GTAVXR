#pragma once
#include "../core/hands.h"
#include "../hands/hand_basis.h"
#include "../xr/locomotion.h"
#include "../body/avatar_pose.h"
namespace gx_vehicle
{
// Seat origin follows the real animated pelvis; eye height is the neutral
// pelvis-to-eye distance. A bike animation's forward head lean is not HMD pose.
inline bool uprightSeatEye(gx_hands::V pelvis, gx_hands::V bindPelvis, gx_hands::V bindEye, gx_hands::V& out)
{
    const double height = bindEye.z - bindPelvis.z;
    if (!gx_avatar::finite(pelvis) || !gx_avatar::finite(bindPelvis) || !gx_avatar::finite(bindEye) ||
        height < .25 || height > 1.1)
        return false;
    out = gx_hands::add(pelvis, gx_hands::V(0, 0, height));
    return gx_avatar::finite(out);
}
// Bootstrap is the measured accepted-runtime18 eye-to-held-wrist-midpoint proxy,
// NOT a claimed exact wheel center (car-reach-proxy.json). Capture of an exact
// settled car eye-to-neutral-control relation supersedes it once, transparently.
inline gx_hands::V carReachBootstrap()
{
    return gx_hands::V(0, -.421, .204);
}
inline bool validCarReach(gx_hands::V v)
{
    return gx_avatar::finite(v) && fabs(v.x) <= .45 && v.y >= -1.0 && v.y <= -.15 && v.z >= .08 && v.z <= .85;
}
inline bool controlSeatEye(gx_hands::V left, gx_hands::V right, gx_hands::V reach, gx_hands::V& out)
{
    if (!gx_avatar::finite(left) || !gx_avatar::finite(right) || !validCarReach(reach))
        return false;
    const gx_hands::V span = gx_hands::sub(left, right);
    const double length = sqrt(gx_hands::dot(span, span));
    if (length < .20 || length > 1.8)
        return false;
    out = gx_hands::add(gx_hands::mul(gx_hands::add(left, right), .5), reach);
    return gx_avatar::finite(out);
}
// Missing or incompatible markers are a native-reference fallback, not a dead
// calibration editor. Wait only for the first identity-qualified geometry sample.
inline bool controlOrNativeSeatEye(bool observed, bool grips, bool nativeAvailable, gx_hands::V left,
                                   gx_hands::V right, gx_hands::V reach, gx_hands::V nativeEye,
                                   gx_hands::V& out, bool& fromControl)
{
    if (!observed)
        return false;
    if (grips && controlSeatEye(left, right, reach, out))
    {
        fromControl = true;
        return true;
    }
    if (!nativeAvailable || !gx_avatar::finite(nativeEye))
        return false;
    out = nativeEye;
    fromControl = false;
    return true;
}
// These offsets move only the initial reference in vehicle/seat local space.
// No HMD quaternion, roomscale delta or animation state is an input/output here.
inline bool localSeatOffset(gx_hands::V base, const int* mm, gx_hands::V& out)
{
    if (!mm || !gx_avatar::finite(base))
        return false;
    for (unsigned c = 0; c < 3; ++c)
        if (mm[c] < -500 || mm[c] > 500)
            return false;
    out = gx_hands::add(base, gx_hands::V(mm[0] * .001, mm[1] * .001, mm[2] * .001));
    return gx_avatar::finite(out);
}
// Software centering response, NOT force feedback. Strictly monotonic, no dead
// zone and full travel preserved. Cars intentionally retain the linear contract.
inline double centered(double x)
{
    x = gx_loco::clamp(x, -1, 1);
    return x * (.45 + .55 * x * x);
}
inline double uncentered(double y)
{
    double lo = -1, hi = 1;
    y = gx_loco::clamp(y, -1, 1);
    for (unsigned n = 0; n < 40; ++n)
    {
        const double x = (lo + hi) * .5;
        if (centered(x) < y)
            lo = x;
        else
            hi = x;
    }
    return (lo + hi) * .5;
}
// Freeze only an actual, settled anatomical sample, never the entry animation.
class SeatReference
{
    gx_hands::V prior, origin;
    unsigned stable;
    bool ready;
    uint64_t first, last;

  public:
    SeatReference() : stable(0), ready(false), first(0), last(0) {}
    void reset()
    {
        stable = 0;
        ready = false;
        first = last = 0;
    }
    bool sample(gx_hands::V eye, bool entering, uint64_t now)
    {
        if (ready)
            return true;
        if (entering || !now || !gx_loco::finite(eye.x) || !gx_loco::finite(eye.y) || !gx_loco::finite(eye.z))
        {
            reset();
            return false;
        }
        const gx_hands::V d = gx_hands::sub(eye, prior);
        if (!stable || now < last || now - last > 250 || d.x * d.x + d.y * d.y + d.z * d.z > .0001)
        {
            stable = 1;
            prior = eye;
            first = last = now;
            return false;
        }
        if (now > last)
        {
            ++stable;
            last = now;
        }
        if (stable >= 3 && now - first >= 200)
        {
            origin = eye;
            ready = true;
        }
        return ready;
    }
    gx_hands::V get() const
    {
        return origin;
    }
    bool isReady() const
    {
        return ready;
    }
};
 
inline unsigned propSet(unsigned count, bool scene, bool xr)
{
    return count == 2 ? (scene && xr ? 1 : 0) : (count == 3 ? (scene ? 2 : 1) : 0);
}
// Vehicle-local rotational control. Capture the arbitrary controller grip basis
// on grab; its fixed ergonomic pitch/roll is not a permanent steering input.
class FlightStick
{
    gx_hands::V initial[3];
    bool held;
    uint64_t identity;

  public:
    FlightStick() : held(false), identity(0) {}
    void reset()
    {
        held = false;
        identity = 0;
    }
    bool gripping() const
    {
        return held;
    }
    bool update(uint64_t vehicle, bool eligible, float grip, const double* m, double& roll, double& pitch,
                double maxRoll = .5235987755982989, double maxPitch = .5235987755982989)
    {
        if (!vehicle || !eligible || !gx_loco::finite(grip) || !m || grip < (held ? .45f : .65f))
        {
            reset();
            return false;
        }
        if (!gx_loco::finite(maxRoll) || !gx_loco::finite(maxPitch) || maxRoll <= 0 || maxPitch <= 0)
        {
            reset();
            return false;
        }
        gx_hands::V basis[3];
        for (unsigned c = 0; c < 3; ++c)
        {
            basis[c] = gx_hands::V(m[c * 4], m[c * 4 + 1], m[c * 4 + 2]);
            if (!gx_loco::finite(basis[c].x) || !gx_loco::finite(basis[c].y) ||
                !gx_loco::finite(basis[c].z) || fabs(gx_hands::dot(basis[c], basis[c]) - 1) > .01)
            {
                reset();
                return false;
            }
        }
        if (fabs(gx_hands::dot(basis[0], basis[1])) > .01 || fabs(gx_hands::dot(basis[0], basis[2])) > .01 ||
            fabs(gx_hands::dot(basis[1], basis[2])) > .01)
        {
            reset();
            return false;
        }
        if (gx_hands::dot(gx_hands::cross(basis[0], basis[1]), basis[2]) < .99)
        {
            reset();
            return false;
        }
        if (!held || vehicle != identity)
        {
            for (unsigned c = 0; c < 3; ++c)
                initial[c] = basis[c];
            identity = vehicle;
            held = true;
        }
        gx_hands::V x, y, z;
        for (unsigned c = 0; c < 3; ++c)
        {
            x = gx_hands::add(x, gx_hands::mul(basis[c], initial[c].x));
            y = gx_hands::add(y, gx_hands::mul(basis[c], initial[c].y));
            z = gx_hands::add(z, gx_hands::mul(basis[c], initial[c].z));
        }
        // Native controls animate about their local X/Y axes. Aircraft caller
        // supplies the actual source-native full-travel angles, not an HMD zone.
        roll = gx_loco::clamp(atan2(-x.z, z.z) / maxRoll, -1, 1);
        pitch = gx_loco::clamp(atan2(y.z, y.y) / maxPitch, -1, 1);
        return true;
    }
};
}
