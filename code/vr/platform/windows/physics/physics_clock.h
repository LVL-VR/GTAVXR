#pragma once
// build147 A (user 146: arms through walls, no door/vehicle resistance, HIGGS jitter; CONFIRMED by the user's 60 FPS
// limiter test). Physics/strike/contact time deltas must come from a high-resolution monotonic clock. GetTickCount64
// advances in ~15.6 ms steps: above ~64 FPS two consecutive updates can share one tick (dt = 0 -> DISCONTINUITY ->
// contact/hold reset). Millisecond ticks stay valid ONLY for coarse freshness/age windows (>= 100 ms).
#include <windows.h>
namespace gx_clock
{
inline double seconds()
{
    static volatile LONGLONG frequency = 0;
    LARGE_INTEGER f, c;
    if (!frequency)
    {
        if (!QueryPerformanceFrequency(&f) || f.QuadPart <= 0)
            return 0;
        frequency = f.QuadPart;
    }
    if (!QueryPerformanceCounter(&c))
        return 0;
    return double(c.QuadPart) / double(frequency);
}
inline unsigned long long micros()
{
    const double s = seconds();
    return s > 0 ? (unsigned long long)(s * 1e6) : 0;
}
}
