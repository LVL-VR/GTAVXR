#pragma once
// Original GTAVXR bounded user settings; presentation/tracking has no gain.
#include "tracked_physics.h"
namespace gx_physical_tuning
{
struct Settings
{
    int grabPercent, pushPercent, bodyPercent;
    Settings() : grabPercent(100), pushPercent(100), bodyPercent(100) {}
    bool same(const Settings& o) const
    {
        return grabPercent == o.grabPercent && pushPercent == o.pushPercent && bodyPercent == o.bodyPercent;
    }
};
enum
{
    MIN_PERCENT = 25,
    MAX_PERCENT = 250,
    BODY_MAX_PERCENT = 500,
    STEP_PERCENT = 25
};
inline int validatedPercent(int value)
{
    return value >= MIN_PERCENT && value <= MAX_PERCENT ? value : 100;
}
inline int validatedBodyPercent(int value)
{
    return value >= MIN_PERCENT && value <= BODY_MAX_PERCENT ? value : 100;
}
inline int adjustBodyPercent(int value, int direction)
{
    const int safe = validatedBodyPercent(value);
    return direction > 0   ? (safe > BODY_MAX_PERCENT - STEP_PERCENT ? BODY_MAX_PERCENT : safe + STEP_PERCENT)
           : direction < 0 ? (safe < MIN_PERCENT + STEP_PERCENT ? MIN_PERCENT : safe - STEP_PERCENT)
                           : safe;
}
inline int adjustPercent(int value, int direction)
{
    const int safe = validatedPercent(value);
    return direction > 0   ? (safe > MAX_PERCENT - STEP_PERCENT ? MAX_PERCENT : safe + STEP_PERCENT)
           : direction < 0 ? (safe < MIN_PERCENT + STEP_PERCENT ? MIN_PERCENT : safe - STEP_PERCENT)
                           : safe;
}
inline Settings checked(const Settings& s)
{
    Settings out;
    out.grabPercent = validatedPercent(s.grabPercent);
    out.pushPercent = validatedPercent(s.pushPercent);
    out.bodyPercent = validatedBodyPercent(s.bodyPercent);
    return out;
}
inline gx_tracked_physics::DriveLimits driveLimits(const Settings& s)
{
    gx_tracked_physics::DriveLimits out;
    const double gain = validatedPercent(s.grabPercent) * .01;
    out.force *= gain;
    out.acceleration *= gain;
    out.torque *= gain;
    // Keep the existing finite motor contract. Increasing strength cannot make
    // the packet invalid or silently remove acceleration/torque safeguards.
    if (out.force > 600)
        out.force = 600;
    if (out.acceleration > 60)
        out.acceleration = 60;
    if (out.torque > 80)
        out.torque = 80;
    // Response rate, tracking, mass, motor saturation and reference-velocity guards
    // are not user-scaled. Above150% only remaining finite force/torque headroom
    // increases; default100% and all existing50..150% results remain unchanged.
    return out;
}
inline gx_physics::V pushForce(const Settings& s, const gx_physics::Impact& impact, double mass, bool door,
                               unsigned holders, gx_physics::V point, gx_physics::V center)
{
    return gx_physics::mul(gx_tracked_physics::pushForce(impact, mass, door, holders, point, center),
                           validatedPercent(s.pushPercent) * .01);
}
// A single copied snapshot for BOTH final owners avoids per-hand budget changes
// during a frame. Caller snapshots under its existing InputGuard; no nested lock,
// INI access, native query, pointers or tracking write exists in this accessor.
struct Snapshot
{
    Settings settings;
    gx_tracked_physics::DriveLimits grab;
    explicit Snapshot(const Settings& s) : settings(checked(s)), grab(driveLimits(settings)) {}
};
}
