#pragma once
#ifndef GTAVXR_PACING_POLICY_H
#define GTAVXR_PACING_POLICY_H
 
// F0: always-on per-frame cost windows (QPC stamps on the update/render threads + per-eye GPU timestamp
//     SPANS, which include GPU idle while the render thread waits for drawlists), summarised every 600
//     frames. Render-thread eye spans likewise include waiting for the update thread to build (streaming).
//     Measurement only: never changes behaviour.
 
 
//     executes the frame's drawlists as they are built (blocking Pop until EndRender's terminator).
//     SERIAL  = build138 exactly: forced native Flush(true) = 4 extra render cycles polled with sysIpcSleep(1).
//     LEAN    = no extra native cycle: AfterRender waits for the exact pair-completion event (EndRenderPair on
//               the render thread, or the native discard callback); the event is reset in PrepareRender,
//               before any drawlist of the pair exists. The render protocol stays the retail one cycle/frame.
//     OVERLAP = LEAN, but the wait moves to the next UpdateEntered so the native end-of-frame work
//               (Present tail, audio wait, frame begin) overlaps the pair on the render thread.
//     TWO_FRAMES (build143 P2, user: "SIII DOS FOTOGRAMAS EN VUELO ESTA APROBADO") = OVERLAP, but an ELIGIBLE plain
//               world pair is completed at the start of the NEXT camera preparation (before its XR begin) instead of
//               UpdateEntered, so scripts/peds/physics of the next update overlap the pair. Ineligible pairs (menu,
//               phone, pause, scope, crop optics, no concurrent-input bridge) run as OVERLAP.
//     Unusual native state or hybrid shared stages -> SERIAL for that pair; a completion timeout drains with
//     the exact build138 Flush(true) (never a guess) and three of them pin SERIAL for the session.
#include <stdio.h>
#include <string.h>

namespace gx_pacing
{

enum Pacing
{
    SERIAL = 0,
    LEAN = 1,
    OVERLAP = 2,
    TWO_FRAMES = 3
};
const unsigned TimeoutMs = 500, TimeoutsToPinSerial = 3;

inline unsigned clampPacing(int value)
{
    return value < 0 ? (unsigned)SERIAL : (value > 3 ? (unsigned)OVERLAP : (unsigned)value);
}
// VR Menu cycle: OVERLAP -> TWO_FRAMES -> LEAN -> SERIAL -> OVERLAP.
inline unsigned nextPacing(unsigned p)
{
    return p == OVERLAP
               ? (unsigned)TWO_FRAMES
               : (p == TWO_FRAMES ? (unsigned)LEAN : (p == LEAN ? (unsigned)SERIAL : (unsigned)OVERLAP));
}
inline const char* pacingName(unsigned p, bool spanish)
{
    if (p == TWO_FRAMES)
        return spanish ? "DOS FOTOGRAMAS" : "TWO FRAMES";
    if (p == OVERLAP)
        return spanish ? "SOLAPADA" : "OVERLAP";
    if (p == LEAN)
        return spanish ? "LIGERA" : "LEAN";
    return spanish ? "SERIE" : "SERIAL";
}

struct PathInput
{
    unsigned requested;
    bool forcedSerial, initializing, renderThreadActive, deviceReady, defaultRenderFunction, eventReady,
        hybrid;
    bool twoFramesEligible; // build143 P2: this pair may complete under the next update (engine eligibility)
    PathInput()
        : requested(SERIAL), forcedSerial(false), initializing(false), renderThreadActive(false),
          deviceReady(false), defaultRenderFunction(false), eventReady(false), hybrid(false),
          twoFramesEligible(false)
    {
    }
};
// LEAN/OVERLAP only for the plain dual pair under the default native render function.
inline unsigned path(const PathInput& in)
{
    if (in.forcedSerial || in.initializing || !in.renderThreadActive || !in.deviceReady ||
        !in.defaultRenderFunction || !in.eventReady || in.hybrid || in.requested > TWO_FRAMES)
        return SERIAL;
    if (in.requested == TWO_FRAMES)
        return in.twoFramesEligible ? (unsigned)TWO_FRAMES : (unsigned)OVERLAP;
    return in.requested;
}

enum Metric
{
    FRAME,
    SIM,
    XR_BEGIN,
    UPDATE_REST,
    BUILD,
    BUILD_EYE0,
    BUILD_EYE1,
    BUILD_TAIL,
    DRAIN,
    COMPLETE,
    TAIL,
    DEFERRED,
    RT_LATENCY,
    RT_EYE0,
    RT_EYE1,
    RT_TAIL,
    RT_CAPTURE,
    GPU_EYE0,
    GPU_EYE1,
    GPU_PAIR,
    METRIC_COUNT
};
inline const char* metricName(unsigned m)
{
    static const char* const names[METRIC_COUNT] = {
        "frame",      "sim",     "xr_begin",   "update_rest", "build",    "build_e0",   "build_e1",
        "build_tail", "drain",   "complete",   "tail",        "deferred", "rt_latency", "rt_e0",
        "rt_e1",      "rt_tail", "rt_capture", "gpu_e0",      "gpu_e1",   "gpu_pair"};
    return m < METRIC_COUNT ? names[m] : "?";
}

struct Stat
{
    double sum, peak;
    unsigned n;
    Stat() : sum(0), peak(0), n(0) {}
    void add(double v)
    {
        if (!(v >= 0.0) || v > 10000.0)
            return;
        sum += v;
        if (v > peak)
            peak = v;
        ++n;
    }
    double avg() const
    {
        return n ? sum / n : 0.0;
    }
    void reset()
    {
        sum = peak = 0;
        n = 0;
    }
    void merge(const Stat& o)
    {
        sum += o.sum;
        if (o.peak > peak)
            peak = o.peak;
        n += o.n;
    }
};

// Update-thread stamps of one frame (QPC ticks; 0 = not reached this frame).
enum Stamp
{
    T_UPDATE,
    T_DEFER0,
    T_DEFER1,
    T_BEGIN0,
    T_BEGIN1,
    T_RENDER,
    T_BUILD0,
    T_EYE0,
    T_EYE1,
    T_BUILD1,
    T_AFTER0,
    T_DRAINED,
    T_COMPLETE,
    T_AFTER1,
    STAMP_COUNT
};
enum PairKind
{
    NO_PAIR = 0,
    WORLD_PAIR = 1,
    PANEL_PAIR = 2
};
struct FrameStamps
{
    long long t[STAMP_COUNT];
    unsigned path, kind;
    bool fallback, discard;
    bool
        lateWait; // build143 P2: T_DEFER0/1 is the wait at camera preparation (inside SIM), not at UpdateEntered
    FrameStamps()
    {
        reset();
    }
    void reset()
    {
        for (unsigned i = 0; i < STAMP_COUNT; ++i)
            t[i] = 0;
        path = SERIAL;
        kind = NO_PAIR;
        fallback = discard = lateWait = false;
    }
};
// Render-thread stamps of the pair; written ONLY while the pair executes, read after its drain.
struct RenderStamps
{
    long long eye0, eye1, end, capture;
    unsigned captures;
    RenderStamps()
    {
        reset();
    }
    void reset()
    {
        eye0 = eye1 = end = capture = 0;
        captures = 0;
    }
};

inline double ms(long long from, long long to, long long frequency)
{
    if (!from || !to || to < from || frequency <= 0)
        return -1.0;
    return double(to - from) * 1000.0 / double(frequency);
}

struct Window
{
    Stat s[METRIC_COUNT];
    unsigned frames, pairs, serial, lean, overlap, two, fallbacks, discards, gpuDropped;
    Window()
    {
        reset();
    }
    void reset()
    {
        for (unsigned i = 0; i < METRIC_COUNT; ++i)
            s[i].reset();
        frames = pairs = serial = lean = overlap = two = fallbacks = discards = gpuDropped = 0;
    }
};

// Closes one frame. nextUpdate = the next UpdateEntered stamp (end of this frame).
inline void accumulate(Window& w, const FrameStamps& f, const RenderStamps& r, long long nextUpdate,
                       long long q)
{
    const long long* t = f.t;
    ++w.frames;
    w.s[FRAME].add(ms(t[T_UPDATE], nextUpdate, q));
    if (f.lateWait)
    { // build143 P2: the wait sits inside the update -> simulation = update..begin minus that wait
        const double all = ms(t[T_UPDATE], t[T_BEGIN0], q), wait = ms(t[T_DEFER0], t[T_DEFER1], q);
        w.s[SIM].add(all >= 0 && wait >= 0 && wait <= all ? all - wait : -1.0);
    }
    else
        w.s[SIM].add(ms(t[T_DEFER1] ? t[T_DEFER1] : t[T_UPDATE], t[T_BEGIN0], q));
    w.s[XR_BEGIN].add(ms(t[T_BEGIN0], t[T_BEGIN1], q));
    w.s[UPDATE_REST].add(ms(t[T_BEGIN1], t[T_RENDER], q));
    w.s[DEFERRED].add(ms(t[T_DEFER0], t[T_DEFER1], q));
    if (f.fallback)
        ++w.fallbacks;
    if (f.discard)
        ++w.discards;
    if (f.kind == NO_PAIR)
        return;
    ++w.pairs;
    if (f.path == OVERLAP)
        ++w.overlap;
    else if (f.path == TWO_FRAMES)
        ++w.two;
    else if (f.path == LEAN)
        ++w.lean;
    else
        ++w.serial;
    if (f.path != OVERLAP && f.path != TWO_FRAMES)
        w.s[DRAIN].add(ms(t[T_AFTER0], t[T_DRAINED], q)); // they wait in the next DEFERRED
    w.s[COMPLETE].add(ms(t[T_DRAINED], t[T_COMPLETE], q));
    w.s[TAIL].add(ms(t[T_AFTER1], nextUpdate, q));
    if (f.kind != WORLD_PAIR)
        return;
    w.s[BUILD].add(ms(t[T_BUILD0], t[T_BUILD1], q));
    w.s[BUILD_EYE0].add(ms(t[T_BUILD0], t[T_EYE0], q));
    w.s[BUILD_EYE1].add(ms(t[T_EYE0], t[T_EYE1], q));
    w.s[BUILD_TAIL].add(ms(t[T_BUILD1], t[T_AFTER0], q));
    w.s[RT_LATENCY].add(
        ms(t[T_BUILD0], r.eye0, q)); // render reaches eye 0 while the pair is still being built
    w.s[RT_EYE0].add(ms(r.eye0, r.eye1, q));
    w.s[RT_EYE1].add(ms(r.eye1, r.end, q));
    w.s[RT_TAIL].add(ms(t[T_BUILD1], r.end, q)); // ~0: render keeps up with the build; large: render-bound
    if (r.captures && q > 0)
        w.s[RT_CAPTURE].add(double(r.capture) * 1000.0 / double(q));
}

// One bounded log line: "event=PERF139 ... name=avg/max ..." (ms, 2 decimals). Returns chars written or -1.
inline int format(const Window& w, unsigned requested, bool forcedSerial, const char* config, char* out,
                  size_t size)
{
    if (!out || size < 64)
        return -1;
    int n = _snprintf_s(
        out, size, _TRUNCATE,
        "event=PERF139 config=%s pacing=%s forced_serial=%u frames=%u pairs=%u serial=%u lean=%u overlap=%u two=%u fallback=%u discard=%u gpu_dropped=%u",
        config ? config : "?", pacingName(requested, false), forcedSerial ? 1u : 0u, w.frames, w.pairs,
        w.serial, w.lean, w.overlap, w.two, w.fallbacks, w.discards, w.gpuDropped);
    if (n < 0)
        return -1;
    for (unsigned m = 0; m < METRIC_COUNT; ++m)
    {
        const int k = _snprintf_s(out + n, size - (size_t)n, _TRUNCATE, " %s=%.2f/%.2f", metricName(m),
                                  w.s[m].avg(), w.s[m].peak);
        if (k < 0)
            return -1;
        n += k;
    }
    const int k = _snprintf_s(out + n, size - (size_t)n, _TRUNCATE, " gpu_n=%u\n", w.s[GPU_PAIR].n);
    return k < 0 ? -1 : n + k;
}

} // namespace gx_perf139
#endif // GTAVXR_PERF139_H
