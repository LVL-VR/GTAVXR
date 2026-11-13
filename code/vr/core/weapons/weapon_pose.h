#pragma once
// Shared, source-independent tracked direction/calibration contracts (v110 compatible).
#include "../body/avatar_pose.h"
#include "../core/hands.h"
namespace gx_weapon
{
enum VehicleMode
{
    NO_VEHICLE_WEAPON,
    HANDHELD,
    MOUNTED
};
inline VehicleMode vehicleMode(bool handheld, bool mounted)
{
    return mounted ? MOUNTED : (handheld ? HANDHELD : NO_VEHICLE_WEAPON);
}
inline bool aim(const double* m, gx_hands::V& direction)
{
    if (!m)
        return false;
    direction = gx_hands::V(-m[8], -m[9], -m[10]);
    return gx_avatar::finite(direction) && gx_hands::unit(direction);
}
inline bool support(const double* right, const double* left, gx_hands::V& direction)
{
    if (!right || !left)
        return false;
    direction = gx_hands::V(left[12] - right[12], left[13] - right[13], left[14] - right[14]);
    return gx_avatar::finite(direction) && gx_avatar::length(direction) >= .10 && gx_hands::unit(direction);
}
inline bool aimPoint(const double* m, double distance, gx_hands::V& point)
{
    gx_hands::V direction;
    if (!m || !_finite(distance) || distance <= 0 || !aim(m, direction))
        return false;
    point = gx_hands::add(gx_hands::V(m[12], m[13], m[14]), gx_hands::mul(direction, distance));
    return gx_avatar::finite(point);
}
// User fine adjustment rotates orientation about the grip's local X, never its position.
inline bool pitch(double* m, int degrees)
{
    if (!m || degrees < -90 || degrees > 90)
        return false;
    for (unsigned n = 0; n < 16; ++n)
        if (!_finite(m[n]))
            return false;
    const double a = degrees * 3.14159265358979323846 / 180, c = cos(a), s = sin(a);
    for (unsigned n = 0; n < 3; ++n)
    {
        const double y = m[4 + n], z = m[8 + n];
        m[4 + n] = c * y + s * z;
        m[8 + n] = -s * y + c * z;
    }
    return true;
}
class ThrowRelease
{
    bool held_, ready_;
    uint64_t owner_, weapon_, sequence_;
    GxHandTargets release_;
    gx_hands::V direction_;

  public:
    ThrowRelease() : held_(false), ready_(false), owner_(0), weapon_(0), sequence_(0)
    {
        release_ = GxHandTargets();
    }
    void reset()
    {
        held_ = ready_ = false;
        owner_ = weapon_ = sequence_ = 0;
        release_ = GxHandTargets();
    }
    bool update(uint64_t owner, uint64_t weapon, float trigger, bool eligible, const GxHandTargets& grip,
                const GxHandTargets& aims, unsigned hand = 1)
    {
        if (hand > 1 || !eligible || !owner || !weapon || !_finite(trigger) ||
            !(grip.valid_mask & (1u << hand)) || !(aims.valid_mask & (1u << hand)) || !grip.frame ||
            !grip.generation || !grip.display_time || grip.frame != aims.frame ||
            grip.generation != aims.generation || grip.display_time != aims.display_time ||
            !_finite(grip.world[hand][12]) || !_finite(grip.world[hand][13]) ||
            !_finite(grip.world[hand][14]))
        {
            reset();
            return false;
        }
        if (owner != owner_ || weapon != weapon_)
        {
            reset();
            owner_ = owner;
            weapon_ = weapon;
        }
        if (trigger >= .55f)
        {
            held_ = true;
            ready_ = false;
            return false;
        }
        if (!held_ || trigger > .2f)
            return false;
        gx_hands::V d;
        if (!aim(aims.world[hand], d))
        {
            reset();
            return false;
        }
        held_ = false;
        ready_ = true;
        release_ = grip;
        direction_ = d;
        ++sequence_;
        return true;
    }
    bool take(uint64_t owner, uint64_t weapon, GxHandTargets& out, gx_hands::V& direction)
    {
        if (!ready_ || owner != owner_ || weapon != weapon_)
            return false;
        out = release_;
        direction = direction_;
        ready_ = false;
        return true;
    }
    bool pending(uint64_t owner, uint64_t weapon) const
    {
        return ready_ && owner == owner_ && weapon == weapon_;
    }
    uint64_t sequence() const
    {
        return sequence_;
    }
};
}
