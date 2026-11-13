#pragma once
 
// No rompamos lo que ya funciona con melee"). Which held weapon may make a PHYSICAL armed strike / world contact.
// MELEE   = the exact build105..148 rule, unchanged (melee, not unarmed/gun/thrown, FIRE_TYPE_MELEE).
 
//           DAR_armed_melee + sound MELEE_COMBAT_POOL_CUE with the EQUIPPED gun's hash and DF_MeleeDamage.
// Thrown, unarmed and vehicle weapons never qualify. Pure: no game types, compiled by the CPU controls.
namespace gx_armed
{
enum Kind
{
    NONE = 0,
    MELEE = 1,
    FIREARM = 2
};
struct Flags
{
    bool melee, unarmed, gun, thrown, fireTypeMelee, vehicle;
    Flags() : melee(false), unarmed(false), gun(false), thrown(false), fireTypeMelee(false), vehicle(false) {}
};
inline Kind kind(const Flags& f)
{
    if (f.unarmed || f.thrown || f.vehicle)
        return NONE;
    if (f.melee && !f.gun && f.fireTypeMelee)
        return MELEE;
    if (f.gun && !f.melee)
        return FIREARM;
    return NONE;
}
// Native damage/reaction table name per kind (MELEE keeps the build144 class rule; unknown -> NULL).
inline const char* reactionName(Kind k, bool blade, bool club)
{
    if (k == MELEE)
        return blade ? "DAR_knife" : club ? "DAR_bat" : "DAR_small_wpn";
    if (k == FIREARM)
        return "DAR_armed_melee";
    return 0;
}
// Trigger is FIRE on a gun: a gun-butt strike is never a trigger "power" strike.
inline bool powerAllowed(Kind k)
{
    return k == MELEE;
}
 
inline bool forceDamageAllowed(Kind k, bool mission)
{
    return k == MELEE && !mission;
}
}
