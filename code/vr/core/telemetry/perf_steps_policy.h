#pragma once
 
#include <string.h>
namespace gx_perf_steps
{
const unsigned RingSize = 8, MaxRegions = 64, MaxSteps = 48, MaxDepth = 16;
enum State
{
    Free,
    Open,
    Pending
};
struct Ring
{
    State state[RingSize];
    unsigned long long born[RingSize];
    Ring()
    {
        reset();
    }
    void reset()
    {
        memset(state, 0, sizeof(state));
        memset(born, 0, sizeof(born));
    }
    unsigned acquire(unsigned long long serial)
    {
        for (unsigned i = 0; i < RingSize; ++i)
            if (state[i] == Free)
            {
                state[i] = Open;
                born[i] = serial;
                return i;
            }
        return RingSize; // drop NEW sample; never overwrite an unread query
    }
    void seal(unsigned i)
    {
        if (i < RingSize && state[i] == Open)
            state[i] = Pending;
    }
    bool eligible(unsigned i, unsigned long long now) const
    {
        return i < RingSize && state[i] == Pending && now >= born[i] && now - born[i] >= 2;
    }
    void retire(unsigned i)
    {
        if (i < RingSize)
            state[i] = Free;
    }
};
// A frequency/period change is a transition, not a missed-vsync sample. Off-grid,
// nonmonotonic and subperiod deltas are INVALID, never silently clamped to k1.
inline unsigned cadence(long long previous, long long display, long long previousPeriod, long long period)
{
    if (previous <= 0 || display <= previous || period <= 0 || previousPeriod != period)
        return 0;
    const double ratio = double(display - previous) / double(period);
    if (ratio < 0.75 || ratio > 100000.0)
        return 0;
    const unsigned k = (unsigned)(ratio + 0.5);
    const double error = ratio - double(k);
    return error >= -0.25 && error <= 0.25 ? k : 0;
}
inline bool gpuMs(unsigned long long first, unsigned long long last, unsigned long long frequency, double& ms)
{
    if (!frequency || last < first)
        return false;
    ms = double(last - first) * 1000.0 / double(frequency);
    return true;
}
// Nested step clock: exclusive, with bounded storage and explicit overflow.
struct Steps
{
    const char* name[MaxSteps];
    long long ticks[MaxSteps], last;
    unsigned stack[MaxDepth], n, depth, overflow, suppressed;
    Steps()
    {
        reset(0);
    }
    void reset(long long now)
    {
        memset(ticks, 0, sizeof(ticks));
        n = depth = overflow = suppressed = 0;
        last = now;
    }
    void charge(long long now)
    {
        if (depth && now >= last)
            ticks[stack[depth - 1]] += now - last;
        last = now;
    }
    void push(const char* s, long long now)
    {
        charge(now);
        if (suppressed || depth == MaxDepth)
        {
            ++suppressed;
            ++overflow;
            return;
        }
        unsigned i = 0;
        for (; i < n; ++i)
            if (!strcmp(name[i], s))
                break;
        if (i == n)
        {
            if (n == MaxSteps)
            {
                ++suppressed;
                ++overflow;
                return;
            }
            name[n++] = s;
        }
        stack[depth++] = i;
    }
    void pop(long long now)
    {
        charge(now);
        if (suppressed)
            --suppressed;
        else if (depth)
            --depth;
        else
            ++overflow;
    }
    unsigned worst() const
    {
        unsigned best = 0;
        for (unsigned i = 1; i < n; ++i)
            if (ticks[i] > ticks[best])
                best = i;
        return best;
    }
};
}
