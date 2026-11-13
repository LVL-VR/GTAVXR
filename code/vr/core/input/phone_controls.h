#pragma once
// Native phone app/script state remains authoritative. This policy owns only
// Quest RIGHT inputs and emits semantic requests, never a second app system.
#include "../xr/locomotion.h"
#include "single_click_intent.h"
namespace gx_phone
{
struct Output
{
    bool owned, open, close, canNavigate;
    float up, down, left, right, select, back;
    Output()
        : owned(false), open(false), close(false), canNavigate(false), up(0), down(0), left(0), right(0),
          select(0), back(0)
    {
    }
};
class Ownership
{
    bool pending_, draining_, wasNative_, entryNeutral_;
    gx_character::SingleClickIntent clickIntent_;
    double requested_;

  public:
    Ownership() : pending_(false), draining_(false), wasNative_(false), entryNeutral_(false), requested_(0) {}
    void reset()
    {
        pending_ = draining_ = wasNative_ = entryNeutral_ = false;
        requested_ = 0;
        clickIntent_.reset();
    }
    bool owned() const
    {
        return pending_ || wasNative_ || draining_;
    }
    Output update(const GxInput& i, double seconds, bool eligible, bool nativeOpen, bool nativeStowing,
                  bool menu)
    {
        Output o;
        const bool click = (i.buttons & GX_RCLICK) != 0;
        const bool stowing = nativeOpen && nativeStowing; // native flag may outlive the displayed phone
        const bool neutral = !(i.buttons & (GX_A | GX_B | GX_RCLICK)) && fabs(i.stick[1][0]) < .25 &&
                             fabs(i.stick[1][1]) < .25 && i.trigger[1] < .15 && i.grip[1] < .15;
        const bool navigationNeutral = !(i.buttons & (GX_A | GX_B | GX_RCLICK)) &&
                                       fabs(i.stick[1][0]) < .25 && fabs(i.stick[1][1]) < .25;
        if (!eligible || !(i.active_hands & 2) || !_finite(seconds))
        {
            reset();
            return o;
        }
        if (wasNative_ && (!nativeOpen || stowing))
        {
            draining_ = true;
            entryNeutral_ = false;
        }
        wasNative_ = nativeOpen && !stowing;
        if (wasNative_)
            pending_ = false;
        if (pending_ && seconds - requested_ >= 3)
        {
            pending_ = false;
            draining_ = true;
        } // denied by native script, not frozen forever
        if (draining_ && !nativeOpen && neutral)
        {
            draining_ = false;
            entryNeutral_ = false;
        }
        // Wearer-approved exit is native BACK through apps/root/stow. R3 is
        // takeout ONLY and must not emit removal flags or leak to gameplay while
        // already open/pending/stowing/draining. A fresh click after native close
        // and neutral release can reopen; holding R3 through stow cannot reopen.
        // Release-only takeout allows either staggered both-stick-click order
        // to become the special-ability/Dpad chord, with no arbitrary timeout.
        // Native open/pending/stow and any menu/L3 involvement cancel the click.
        const bool request = clickIntent_.update(click, menu || (i.buttons & (GX_LCLICK | GX_MENU)) != 0,
                                                 !draining_ && !nativeOpen && !pending_);
        if (request)
        {
            o.open = true;
            pending_ = true;
            requested_ = seconds;
            entryNeutral_ = false;
        }
        o.owned = owned() || o.open || stowing;
        // Holding the prop with grip/trigger must NOT block all app buttons.
        // Full neutral is required only before restoring right gameplay input.
        if (wasNative_ && navigationNeutral && !draining_)
            entryNeutral_ = true;
        if (wasNative_ && entryNeutral_ && !draining_ && !menu)
        {
            o.canNavigate = true; // same accepted app-input decision, not a HUD guess
            // Native app holds/repeats/back-at-root implement navigation/lifecycle.
            const float x = i.stick[1][0], y = i.stick[1][1];
            if (_finite(x) && _finite(y))
            {
                if (fabs(x) > fabs(y))
                {
                    o.left = x < -.45f ? 1.f : 0.f;
                    o.right = x > .45f ? 1.f : 0.f;
                }
                else
                {
                    o.up = y > .45f ? 1.f : 0.f;
                    o.down = y < -.45f ? 1.f : 0.f;
                }
            }
            o.select = (i.buttons & GX_A) ? 1.f : 0.f;
            o.back = (i.buttons & GX_B) ? 1.f : 0.f;
        }
        return o;
    }
    static void filter(GxInput& input, bool owned)
    {
        if (!owned)
            return;
        input.buttons &= ~(GX_A | GX_B | GX_RCLICK);
        input.stick[1][0] = input.stick[1][1] = input.trigger[1] = input.grip[1] = 0;
        // Left stick, buttons, trigger, grip and BOTH tracking poses untouched.
    }
};
}
