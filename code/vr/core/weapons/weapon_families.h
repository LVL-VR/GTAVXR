#pragma once
// Equipment families only. No weapon state, ammo, fire or damage simulation.
#include <float.h>
namespace gx_weapon_family
{
enum Fire
{
    HIT,
    DELAYED,
    PROJECTILE,
    MELEE,
    OTHER
};
enum Ammo
{
    NO_AMMO,
    PROJECTILE_AMMO,
    ROCKET_AMMO,
    OTHER_AMMO,
    THROWN_AMMO
};
struct Facts
{
    Fire fire;
    Ammo ammo;
    bool unarmed, gun, melee, noModel, remove, visible, thrown, cook;
    float chargedLaunchTime;
    Facts()
        : fire(OTHER), ammo(NO_AMMO), unarmed(false), gun(false), melee(false), noModel(false), remove(false),
          visible(false), thrown(false), cook(false), chargedLaunchTime(-1)
    {
    }
};
inline bool legacyEquip(const Facts& f)
{
    return f.gun && (f.fire == HIT || f.fire == DELAYED) && !f.noModel && !f.visible && !f.remove;
}
inline bool equip(const Facts& f)
{
    if (f.fire == HIT || f.fire == DELAYED)
        return legacyEquip(f); // exact former equipment contract
    if (f.unarmed || f.noModel)
        return false;
    // Native CAmmoThrownInfo derives from projectile, and -1 is the authored
    // uncharged default. Do not confuse subclass identity with unsupported ammo.
    if (f.thrown)
        return f.fire == PROJECTILE && (f.ammo == PROJECTILE_AMMO || f.ammo == THROWN_AMMO) &&
               _finite(f.chargedLaunchTime) && f.chargedLaunchTime >= -1;
    if (f.remove)
        return false;
    if (f.fire == MELEE)
        return f.melee && !f.visible; // real object; no gun Fire
    return f.fire == PROJECTILE && f.gun && !f.cook && (f.ammo == PROJECTILE_AMMO || f.ammo == ROCKET_AMMO) &&
           _finite(f.chargedLaunchTime) && f.chargedLaunchTime <= 0;
}
inline bool cycle(const Facts& f)
{
    return f.fire == HIT || f.fire == DELAYED || (f.fire == PROJECTILE && equip(f));
}
inline bool visual(const Facts& f)
{
    if (f.unarmed)
        return false;
    return f.fire == HIT || f.fire == DELAYED ||
           equip(f); // preserve former HITSCAN visual, not stricter equip metadata
}
inline int aimBone(int projectile, int muzzle, unsigned count)
{
    // Ordnance attachment may have a different authored basis (RPG rocket).
    // The native muzzle is the firing-frame contract; keep projectile fallback
    // only for models without a valid muzzle, not for generic equipment origin.
    if (muzzle >= 0 && (unsigned)muzzle < count)
        return muzzle;
    return projectile >= 0 && (unsigned)projectile < count ? projectile : -1;
}
}
