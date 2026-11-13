#pragma once
#include <stdint.h>
// Deliberate two rising edges within 300 REAL ms, with a release in between.
// Single-click native intent waits only for this disambiguation window; tracking
// and native simulation never wait. Chords/ownership loss cancel through neutral.
namespace gx_character
{
class DoubleClickIntent
{
    bool barrier_, down_, waiting_, released_, doubleHeld_;
    uint64_t first_, last_;

  public:
    enum
    {
        WINDOW_MS = 300
    };
    struct Result
    {
        bool single, doublePress, suppress;
        Result() : single(false), doublePress(false), suppress(false) {}
    };
    DoubleClickIntent()
    {
        reset();
    }
    void reset()
    {
        barrier_ = true;
        down_ = waiting_ = released_ = doubleHeld_ = false;
        first_ = last_ = 0;
    }
    bool pending() const
    {
        return waiting_ || down_;
    }
    Result update(bool down, bool cancel, bool eligible, uint64_t now, bool fresh = true)
    {
        Result out;
        if (!eligible || !now || (last_ && now < last_))
        {
            reset();
            return out;
        }
        last_ = now;
        if (cancel)
        {
            waiting_ = released_ = doubleHeld_ = false;
            barrier_ = down;
            down_ = down;
            return out;
        }
        if (barrier_)
        {
            out.suppress = doubleHeld_;
            if (fresh)
            {
                down_ = down;
                if (!down)
                {
                    barrier_ = doubleHeld_ = false;
                }
            }
            return out;
        }
        // A released first click expires even if the tracker sequence is stable.
        if (waiting_ && released_ && now - first_ > WINDOW_MS)
        {
            out.single = true;
            waiting_ = released_ = false;
        }
        if (!fresh)
            return out;
        const bool rise = down && !down_, fall = !down && down_;
        if (rise)
        {
            if (waiting_ && released_ && now - first_ <= WINDOW_MS)
            {
                out.doublePress = out.suppress = true;
                waiting_ = released_ = false;
                barrier_ = doubleHeld_ = true;
            }
            else
            {
                waiting_ = true;
                released_ = false;
                first_ = now;
            }
        }
        if (fall && waiting_)
        {
            released_ = true;
            if (now - first_ > WINDOW_MS)
            {
                out.single = true;
                waiting_ = released_ = false;
            }
        }
        down_ = down;
        return out;
    }
};
}
