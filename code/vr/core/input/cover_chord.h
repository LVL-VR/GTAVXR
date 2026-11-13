#pragma once
#include "../core/input.h"
// Quest-only face-button arbitration. Physical native sources are not inputs here.
// A short single-button grace period prevents the leading Y/B edge of a deliberate
// chord from also requesting entry/reload. No tracking, axes or trigger is delayed.
namespace gx_cover
{
struct Result
{
    unsigned faces;
    bool cover, consume;
    Result() : faces(0), cover(false), consume(false) {}
};
class Chord
{
    bool barrier_, chord_, drain_;
    unsigned pending_, pass_, raw_, epoch_;
    uint64_t began_[2], now_, sequence_;

  public:
    enum
    {
        GRACE_MS = 80
    };
    Chord()
    {
        reset();
    }
    void reset()
    {
        barrier_ = true;
        chord_ = drain_ = false;
        pending_ = pass_ = raw_ = epoch_ = 0;
        began_[0] = began_[1] = now_ = sequence_ = 0;
    }
    Result update(const GxInput& i, bool eligible, unsigned epoch, uint64_t now)
    {
        const unsigned mask = GX_Y | GX_B, raw = i.buttons & mask;
        Result out;
        out.faces = raw;
        const bool valid = i.size == sizeof(i) && i.version == 1 && i.sequence && (i.active_hands & 3) == 3 &&
                           i.head_valid && epoch && now;
        const bool retiring = chord_ || drain_;
        if (!valid || !eligible)
        {
            reset();
            drain_ = retiring && raw != 0;
            if (retiring)
            {
                out.faces = 0;
                out.consume = true;
            }
            return out;
        }
        if ((epoch_ && epoch_ != epoch) || (sequence_ && i.sequence < sequence_) || (now_ && now < now_))
        {
            reset();
        }
        epoch_ = epoch;
        sequence_ = i.sequence;
        now_ = now;
        if (drain_)
        {
            out.faces = 0;
            out.consume = true;
            if (!raw)
            {
                drain_ = false;
                pending_ = pass_ = raw_ = 0;
            }
            return out;
        }
        if (barrier_)
        {
            out.faces = 0;
            out.consume = raw != 0;
            if (!raw)
                barrier_ = false;
            return out;
        }
        // A higher-priority owner can appear after acquisition too. Revoke cover
        // immediately and drain the consumed faces; raw modifier input remains
        // available to its accepted semantic owner (for example L3+B context).
        if (chord_ && (i.buttons & ~mask))
        {
            chord_ = false;
            drain_ = raw != 0;
            pending_ = pass_ = 0;
            raw_ = raw;
            out.faces = 0;
            out.consume = true;
            return out;
        }
        if (chord_)
        {
            out.faces = 0;
            out.consume = true;
            out.cover = raw == mask;
            if (!out.cover)
            {
                chord_ = false;
                drain_ = raw != 0;
            }
            raw_ = raw;
            return out;
        }
        // A face chord is not acquired alongside phone/menu/context/other face
        // modifiers. Those accepted owners retain their original button levels.
        if (i.buttons & ~mask)
        {
            pending_ = 0;
            pass_ = raw;
            raw_ = raw;
            return out;
        }
        out.faces = 0;
        const unsigned bit[2] = {GX_Y, GX_B};
        for (unsigned n = 0; n < 2; ++n)
        {
            if ((raw_ & bit[n]) && !(raw & bit[n]))
            {
                if (pending_ & bit[n])
                    out.faces |= bit[n]; // single tap, exactly once
                pending_ &= ~bit[n];
                pass_ &= ~bit[n];
            }
            if ((raw & bit[n]) && !(raw_ & bit[n]))
            {
                pending_ |= bit[n];
                began_[n] = now;
            }
        }
        if (raw == mask && pending_ == mask && !pass_ && now - began_[0] <= GRACE_MS &&
            now - began_[1] <= GRACE_MS)
        {
            chord_ = true;
            pending_ = pass_ = 0;
            out.faces = 0;
            out.cover = out.consume = true;
        }
        else
        {
            for (unsigned n = 0; n < 2; ++n)
                if ((pending_ & bit[n]) && now - began_[n] >= GRACE_MS)
                {
                    pass_ |= bit[n];
                    pending_ &= ~bit[n];
                }
            out.faces |= raw & pass_;
        }
        raw_ = raw;
        return out;
    }
};
inline void filter(GxInput& i, const Result& r)
{
    i.buttons = (i.buttons & ~unsigned(GX_Y | GX_B)) | r.faces;
}
}
