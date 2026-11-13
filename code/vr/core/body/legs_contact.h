#pragma once
 
 
// bound: what your feet push). With Piernas suaves on it does not push:
 
//   - any body on the ground: a corpse or a ragdolling ped (NPC or animal).
// Standing NPCs, vehicles, walls, anchored map objects and heavy big objects keep every contact. A body a hand holds never touches
 
// ped on its ground by probes, not by capsule contacts. The arm body has no legs rule (161 G broke the hands and arms).
// No game headers, C++03.
namespace gx_legs_contact
{
struct Object
{
    bool loose, soft, fixed, neverActivate, door, attached, fixedForSmall, anchoredFrag, impossiblePush;
    Object()
        : loose(false), soft(false), fixed(false), neverActivate(false), door(false), attached(false),
          fixedForSmall(false), anchoredFrag(false), impossiblePush(false)
    {
    }
};
struct Body
{
    bool otherPed, localPlayer, held, dead, ragdoll;
    Body() : otherPed(false), localPlayer(false), held(false), dead(false), ragdoll(false) {}
};
 
inline bool ignoreObject(bool fresh, bool enabled, const Object& o)
{
    if (!fresh || !enabled || !o.loose)
        return false;
    // held or closing the grip on it (159 J)
    if (o.soft)
        return true;
    return !o.fixed && !o.neverActivate && !o.door && !o.attached && !o.fixedForSmall && !o.anchoredFrag &&
           !o.impossiblePush;
}
inline bool ignoreBody(bool fresh, bool enabled, const Body& b)
{
    if (!fresh || !b.otherPed || b.localPlayer)
        return false;
    if (b.held)
        return true;
    return enabled && (b.dead || b.ragdoll);
}
// The arm body (hands and arms only): the object a hand holds or is closing its grip on, as in 159 J / 160 (toggle on)
inline bool gripSoft(bool fresh, bool enabled, bool loose, bool soft)
{
    return fresh && enabled && loose && soft;
}
}
