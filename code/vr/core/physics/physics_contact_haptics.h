#pragma once
// Confirmation of an EXISTING D hand-query contact, not damage, body impulse,
// grasp effort or gameplay ownership. Requires the frozen C matched v2 transport.
#include "physics_contact.h"
#include "../input/haptics.h"
#include <stddef.h>

static_assert(sizeof(GxRumble) == 32 && offsetof(GxRumble, spatial) == 24,
              "contact feedback requires the matched C spatial v2 ABI");
namespace gx_contact_haptic
{
enum
{
    MaximumAgeMs = 100,
    MinimumIntervalMs = 150
};
inline float amplitude()
{
    return .12f;
} // at most .24 after the existing 200% user gain
struct Stamp
{
    uintptr_t actor;
    unsigned epoch, handMask;
    uint64_t frame, generation, tick;
    Stamp() : actor(0), epoch(0), handMask(0), frame(0), generation(0), tick(0) {}
    bool usable() const
    {
        return actor && epoch && frame && generation && tick;
    }
    bool owner(const Stamp& s) const
    {
        return actor == s.actor && epoch == s.epoch;
    }
    bool pose(const Stamp& s) const
    {
        return owner(s) && frame == s.frame && generation == s.generation;
    }
};
class Feedback
{
    struct Hand
    {
        bool clearSeen, touching, pending;
        Hand() : clearSeen(false), touching(false), pending(false) {}
    };
    Stamp published_;
    Hand hands_[2];
    uint64_t lastPulse_[2];
    void invalidate(unsigned h)
    {
        hands_[h] = Hand();
    }

  public:
    Feedback()
    {
        lastPulse_[0] = lastPulse_[1] = 0;
    }
    // Keep the per-hand real-time rate limit even across identity/flag resets.
    // Nothing here is an entity pointer to dereference or a simulation write.
    void clear()
    {
        published_ = Stamp();
        invalidate(0);
        invalidate(1);
    }
    void publish(const Stamp& s, const gx_physics::Solved contact[2], const int target[2], unsigned heldMask,
                 bool enabled)
    {
        if (!enabled || !s.usable())
        {
            clear();
            return;
        }
        if (published_.usable())
        {
            if (!published_.owner(s) || s.tick < published_.tick || s.tick - published_.tick > MaximumAgeMs ||
                s.frame < published_.frame || s.generation < published_.generation)
                clear();
            else if (published_.pose(s))
                return; // duplicate UPDATE cannot re-arm or renew
        }
        published_ = s;
        for (unsigned h = 0; h < 2; ++h)
        {
            Hand& state = hands_[h];
            state.pending = false;
            const gx_physics::Solved& v = contact[h];
            if (!(s.handMask & (1u << h)) || (heldMask & (1u << h)) || target[h] != 1 || !v.valid ||
                v.epoch != s.epoch || v.frame != s.frame || v.generation != s.generation)
            {
                invalidate(h);
                continue;
            }
            if (v.status == gx_physics::CLEAR && !v.hit.present)
            {
                state.clearSeen = true;
                state.touching = false;
                continue;
            }
            if (v.status != gx_physics::CONTACT_SOLVED || !v.hit.present || !v.hit.usable() ||
                !v.hit.token.valid())
            {
                invalidate(h);
                continue;
            }
            const bool onset = state.clearSeen && !state.touching;
            state.touching = true;
            state.clearSeen = false;
            if (onset &&
                (!lastPulse_[h] || (s.tick >= lastPulse_[h] && s.tick - lastPulse_[h] >= MinimumIntervalMs)))
            {
                state.pending = true;
                lastPulse_[h] = s.tick;
            }
        }
    }
    // Caller owns InputGuard. Merge once into a LOCAL snapshot after any C
    // weapon-owner filtering and BEFORE enhance; never overwrite rumbleQueue.
    // Disabled/unsafe contact feedback leaves every normal XInput byte intact.
    bool mix(GxRumble& out, const Stamp& current, bool enabled, bool eligible, bool matchedV2)
    {
        if (!enabled || !eligible || !matchedV2 || !current.usable() || !published_.pose(current) ||
            current.tick < published_.tick || current.tick - published_.tick > MaximumAgeMs)
        {
            clear();
            return false;
        }
        if (!gx_haptic::valid(out))
        {
            clear();
            return false;
        }
        bool added = false;
        for (unsigned h = 0; h < 2; ++h)
        {
            if (!(current.handMask & (1u << h)))
            {
                invalidate(h);
                continue;
            }
            if (!hands_[h].pending)
                continue;
            hands_[h].pending = false; // no renewal, including a second reader of the same frame
            if (out.version == 1)
            {
                out.reserved[0] = out.reserved[1] = 0;
                out.version = 2;
            }
            if (out.spatial[h] < amplitude())
                out.spatial[h] = amplitude();
            added = true;
        }
        return added;
    }
};
}
