#pragma once
// Input ownership is not hit admission. A tracked empty/melee hand must never
// leak an authored attack during the chord's rise/fall. Actual native damage
// still needs current contact, motion, identity and a deliberate full chord.
#include "physical_intent.h"
namespace gx_physical_input
{
 
// empty/melee hand is owned even when the controller is untracked or in the shoulder holster zone. Those states only
// stop STRIKES (strikeIntent below), never return RT to native melee, which stays on B. Held props, busy hands
// (wheel/flight/phone/native props) and ineligible states (menus, pause, vehicles...) are unchanged.
inline bool ownsTrigger(const gx_physical_intent::Hand& hand, unsigned side, bool eligible)
{
    if (side > 1 || !eligible || hand.held || hand.busy)
        return false;
    return hand.kind == gx_physical_intent::EMPTY_HAND ||
           hand.kind == gx_physical_intent::TRACKED_MELEE_ITEM ||
           (side == 1 && hand.kind == gx_physical_intent::RIGHT_MELEE_ITEM);
}
inline bool strikeIntent(const gx_physical_intent::Hand& hand, unsigned side, bool eligible)
{
    return ownsTrigger(hand, side, eligible) && hand.tracked && !hand.holsterZone &&
           gx_strike::fistIntent(true, true, hand.grip, hand.trigger);
}
}
