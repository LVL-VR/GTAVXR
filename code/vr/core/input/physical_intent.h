#pragma once
// Quest input arbitration only. Never creates a hit or awards damage/physics ownership.
#include "../physics/physical_strike.h"
namespace gx_physical_intent
{
enum Kind
{
    UNKNOWN_ITEM,
    EMPTY_HAND,
    OTHER_ITEM,
    RIGHT_MELEE_ITEM,
    TRACKED_MELEE_ITEM
}; // legacy values preserved
struct Hand
{
    Kind kind;
    bool tracked, held, busy, holsterZone;
    double grip, trigger;
    Hand()
        : kind(UNKNOWN_ITEM), tracked(false), held(false), busy(false), holsterZone(false), grip(0),
          trigger(0)
    {
    }
};
inline bool reserve(const Hand& hand, unsigned side, bool eligible)
{
    if (side > 1 || !eligible || !hand.tracked || hand.held || hand.busy || hand.holsterZone)
        return false;
    if (hand.kind == EMPTY_HAND)
        return gx_strike::fistIntent(true, true, hand.grip, hand.trigger);
    return (hand.kind == TRACKED_MELEE_ITEM || (side == 1 && hand.kind == RIGHT_MELEE_ITEM)) &&
           gx_physics::finite(hand.grip) && hand.grip >= .65 && hand.grip <= 1;
}
// Idle task != authored attack. Actual action/reaction/native damage still wins.
inline bool nativeOwns(bool actionTask, bool activeActionState, bool actionChild, bool observedNativeDamage)
{
    return actionTask || activeActionState || actionChild || observedNativeDamage;
}
}
