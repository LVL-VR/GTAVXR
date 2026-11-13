#pragma once
// Exact native instance/object membership, never weapon/model hash equality.
// Native adapter supplies fresh values; only native RegdWeapon/RegdObj retain
// objects. A tagged shot whose owner disappeared keeps its captured muzzle.
#include <stdint.h>
namespace gx_bullet_source
{
enum Route
{
    ORIGINAL_PRIMARY,
    EXACT_SOURCE,
    STORED_MUZZLE
};
struct Observation
{
    uintptr_t actor, weapon, object, slotWeapon, slotObject, objectWeapon, weaponObject, attachParent;
    unsigned hand;
    int attachBone, expectedBone;
    bool active;
    Observation()
        : actor(0), weapon(0), object(0), slotWeapon(0), slotObject(0), objectWeapon(0), weaponObject(0),
          attachParent(0), hand(2), attachBone(-1), expectedBone(-1), active(false)
    {
    }
    bool exact() const
    {
        return active && actor && weapon && object && hand < 2 && slotWeapon == weapon &&
               slotObject == object && objectWeapon == weapon && weaponObject == object &&
               attachParent == actor && expectedBone >= 0 && attachBone == expectedBone;
    }
};
inline unsigned capture(const Observation& now)
{
    return now.exact() ? now.hand : 2;
}
inline Route select(unsigned capturedHand, uintptr_t retainedWeapon, uintptr_t retainedObject,
                    const Observation& now)
{
    if (capturedHand >= 2)
        return ORIGINAL_PRIMARY;
    return retainedWeapon && retainedObject && now.exact() && now.hand == capturedHand &&
                   now.weapon == retainedWeapon && now.object == retainedObject
               ? EXACT_SOURCE
               : STORED_MUZZLE;
}
}
