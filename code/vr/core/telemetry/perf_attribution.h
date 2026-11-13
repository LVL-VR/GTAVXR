#pragma once
#ifndef GTAVXR_PERF_ATTRIBUTION_H
#define GTAVXR_PERF_ATTRIBUTION_H
// Performance attribution for paired rendering.

 
 
// iteration; main: one CApp::RunGame loop iteration). Inside a root every QPC tick is charged to exactly ONE scope: the
// innermost open scope (exclusive time). Time with no open scope is RESIDUAL (unattributed). coverage = 1 - (residual +
// unnamed scopes) / root time, per window and per frame (worst frame reported). Parent totals, cross-thread spans and GPU
// times are never added to CPU coverage.
#include <stdio.h>
#include <string.h>

namespace gx_perf_attribution
{

// ---- Scope ids (one id space; each thread uses its own subset).
enum Fixed
{
    RESIDUAL = 0,
    // render thread
    RT_LOOP_HEAD,
    RT_RENDER_FUNCTION,
    RT_FOCUS_PAUSE,
    RT_ALT_RENDER,
    RT_DISCARD,
    RT_PRE_RENDER,
    RT_EXECUTE,
    RT_WAIT_PRODUCER,
    RT_DL_DEPENDENCY,
    RT_POST_RENDER,
    RT_FLIP,
    RT_WAIT_UPDATE,
    RT_SIGNAL,
    RT_BEGIN_RENDER,
    RT_BEGINFRAME_CTX,
    RT_BEGINFRAME_FENCE,
    RT_END_RENDER,
    RT_END_DRAW,
    RT_PRESENT,
    RT_LIMITER,
    RT_XR_CAPTURE,
    RT_SCOPE_SOURCE,
    RT_MIRROR_COPY,
    RT_PERF_SELF,
    // main (update) thread
    M_APP_HEAD,
    M_BEGIN_UPDATE,
    M_GAME_UPDATE,
    M_ADAPTER_ENTRY,
    M_PAIR_EVENT_WAIT,
    M_PAIR_FLUSH,
    M_POST_DRAIN,
    M_XR_FINISH,
    M_XR_BEGIN,
    M_CTX_WAIT,
    M_UPD_PRE,
    M_PREPARE_CAMERA,
    M_PRESCAN,
    M_BUILD_RENDER_LISTS,
    M_WATER_BLOCK,
    M_PRERENDER,
    M_VERLET_WAIT,
    M_OCCLUSION,
    M_FWGAME,
    M_POST_UPDATE,
    M_LATE_PAIR,
    M_GPU_IDLE,
    M_SYNC,
    M_SYNC_WAIT_RENDER,
    M_SYNC_CTX_LOCK,
    M_SYNC_BUFFERS,
    M_SAFE_OPS,
    M_CTRL_LAUNCH,
    M_GAME_RENDER,
    M_PREPARE_RENDER,
    M_RENDER_PHASES,
    M_PAIR_BUILD,
    M_AFTER_RENDER,
    M_CTRL_WAIT,
    M_AUDIO_WAIT,
    M_APP_TAIL,
    M_PERF_SELF,
    FIXED_COUNT
};
inline const char* fixedName(unsigned id)
{
    static const char* const n[FIXED_COUNT] = {"residual",
                                               "rt_loop_head",
                                               "rt_render_function",
                                               "rt_focus_pause",
                                               "rt_alt_render",
                                               "rt_discard",
                                               "rt_pre_render",
                                               "rt_execute",
                                               "rt_wait_producer",
                                               "rt_dl_dependency",
                                               "rt_post_render",
                                               "rt_flip",
                                               "rt_wait_update",
                                               "rt_signal",
                                               "rt_begin_render",
                                               "rt_beginframe_ctx",
                                               "rt_beginframe_fence",
                                               "rt_end_render",
                                               "rt_end_draw",
                                               "rt_present",
                                               "rt_limiter",
                                               "rt_xr_capture",
                                               "rt_scope_source",
                                               "rt_mirror_copy",
                                               "rt_perf_self",
                                               "m_app_head",
                                               "m_begin_update",
                                               "m_game_update",
                                               "m_adapter_entry",
                                               "m_pair_event_wait",
                                               "m_pair_flush",
                                               "m_post_drain",
                                               "m_xr_finish",
                                               "m_xr_begin",
                                               "ctx_wait",
                                               "m_upd_pre",
                                               "m_prepare_camera",
                                               "m_prescan",
                                               "m_build_render_lists",
                                               "m_water_block",
                                               "m_prerender",
                                               "m_verlet_wait",
                                               "m_occlusion",
                                               "m_fwgame",
                                               "m_post_update",
                                               "m_late_pair",
                                               "m_gpu_idle",
                                               "m_sync",
                                               "m_sync_wait_render",
                                               "m_sync_ctx_lock",
                                               "m_sync_buffers",
                                               "m_safe_ops",
                                               "m_ctrl_launch",
                                               "m_game_render",
                                               "m_prepare_render",
                                               "m_render_phases",
                                               "m_pair_build",
                                               "m_after_render",
                                               "m_ctrl_wait",
                                               "m_audio_wait",
                                               "m_app_tail",
                                               "m_perf_self"};
    return id < FIXED_COUNT ? n[id] : "?";
}
// Drawlists (render thread): type x eye slot (0 = outside an eye, 1 = eye 0, 2 = eye 1). Types >= OtherType = unnamed.
const unsigned MaxTypes = 64, OtherType = MaxTypes - 1, EyeSlots = 3;
const unsigned ListBase = FIXED_COUNT;
// Game-skeleton update callbacks (main thread): one slot per atHashString; overflow slot = unnamed.
const unsigned MaxCallbacks = 160;
const unsigned CallbackBase = ListBase + MaxTypes * EyeSlots;
const unsigned CallbackOverflow = CallbackBase + MaxCallbacks - 1;
const unsigned MaxIds = CallbackBase + MaxCallbacks;
inline unsigned eyeSlot(int eye)
{
    return eye == 0 ? 1u : (eye == 1 ? 2u : 0u);
}
inline unsigned listId(unsigned type, int eye)
{
    if (type >= MaxTypes)
        type = OtherType;
    return ListBase + type * EyeSlots + eyeSlot(eye);
}
inline bool isList(unsigned id)
{
    return id >= ListBase && id < CallbackBase;
}
inline bool isNamed(unsigned id)
{
    if (id == RESIDUAL || id >= MaxIds || id == CallbackOverflow)
        return false;
    if (isList(id) && (id - ListBase) / EyeSlots == OtherType)
        return false;
    return true;
}

struct CallbackMap
{ // open addressing, hash 0 never stored
    unsigned hash[MaxCallbacks - 1];
    unsigned used;
    void reset()
    {
        memset(hash, 0, sizeof(hash));
        used = 0;
    }
    // Returns the scope id; newSlot = true the first time a hash is seen (caller logs its name once).
    unsigned id(unsigned h, bool& newSlot)
    {
        newSlot = false;
        if (!h)
            return CallbackOverflow;
        const unsigned n = MaxCallbacks - 1;
        for (unsigned probe = 0, k = h % n; probe < n; ++probe, k = (k + 1) % n)
        {
            if (hash[k] == h)
                return CallbackBase + k;
            if (!hash[k])
            {
                if (used + 1 >= n)
                    return CallbackOverflow;
                hash[k] = h;
                ++used;
                newSlot = true;
                return CallbackBase + k;
            }
        }
        return CallbackOverflow;
    }
};

// ---- Period histogram: 0.1 ms bins up to 100 ms, last bin = overflow.
const unsigned HistBins = 1001;
struct Hist
{
    unsigned bin[HistBins];
    unsigned n;
    double sum, peak;
    void reset()
    {
        memset(bin, 0, sizeof(bin));
        n = 0;
        sum = peak = 0;
    }
    void add(double ms)
    {
        if (!(ms >= 0.0) || ms > 100000.0)
            return;
        unsigned k = (unsigned)(ms * 10.0);
        if (k >= HistBins)
            k = HistBins - 1;
        ++bin[k];
        ++n;
        sum += ms;
        if (ms > peak)
            peak = ms;
    }
    double avg() const
    {
        return n ? sum / n : 0.0;
    }
    double quantile(double q) const
    { // upper edge of the bin holding the q-quantile (conservative)
        if (!n)
            return 0.0;
        unsigned want = (unsigned)(q * n);
        if (want >= n)
            want = n - 1;
        unsigned seen = 0;
        for (unsigned k = 0; k < HistBins; ++k)
        {
            seen += bin[k];
            if (seen > want)
                return k + 1 < HistBins ? (k + 1) / 10.0 : peak;
        }
        return peak;
    }
    unsigned over(double ms) const
    {
        unsigned k0 = (unsigned)(ms * 10.0) + 1, c = 0;
        for (unsigned k = k0; k < HistBins; ++k)
            c += bin[k];
        return c;
    }
};

// ---- The five longest roots of a window with their dominant scope.
struct Hitch
{
    double period, dominantMs;
    unsigned dominant;
};
struct Top5
{
    Hitch h[5];
    unsigned n;
    void reset()
    {
        n = 0;
    }
    void offer(double period, unsigned dominant, double dominantMs)
    {
        unsigned at = 5;
        for (unsigned i = 0; i < n; ++i)
            if (period > h[i].period)
            {
                at = i;
                break;
            }
        if (at == 5)
        {
            if (n >= 5)
                return;
            at = n;
        }
        for (unsigned j = (n < 5 ? n : 4); j > at; --j)
            h[j] = h[j - 1];
        h[at].period = period;
        h[at].dominant = dominant;
        h[at].dominantMs = dominantMs;
        if (n < 5)
            ++n;
    }
};

const unsigned MaxStamps = 256;
struct GpuFrame
{
    unsigned n;
    unsigned short id[MaxStamps];
};

// ---- One thread's exclusive scope profiler.
struct Window
{
    double cpu[MaxIds];
    double gpu[MaxIds];
    Hist period;
    Top5 top;
    unsigned frames, mismatches, overflowDepth, gpuFrames, gpuDropped, gpuDisjoint;
    double total, unnamed, worstUncovered, gpuSpan;
    void reset()
    {
        memset(cpu, 0, sizeof(cpu));
        memset(gpu, 0, sizeof(gpu));
        period.reset();
        top.reset();
        frames = mismatches = overflowDepth = gpuFrames = gpuDropped = gpuDisjoint = 0;
        total = unnamed = worstUncovered = gpuSpan = 0;
    }
};
const unsigned MaxDepth = 32;
struct Profiler
{
    unsigned stack[MaxDepth];
    unsigned depth;
    int eye;
    long long last, rootStart, q;
    bool started;
    double frame[MaxIds];
    Window w;
    void reset(long long frequency)
    {
        depth = 0;
        eye = -1;
        last = rootStart = 0;
        q = frequency;
        started = false;
        memset(frame, 0, sizeof(frame));
        w.reset();
    }
    unsigned top() const
    {
        return depth ? stack[depth - 1] : (unsigned)RESIDUAL;
    }
    void close(long long now)
    {
        if (started && q > 0 && now >= last)
        {
            const unsigned id = top();
            frame[id < MaxIds ? id : (unsigned)RESIDUAL] += double(now - last) * 1000.0 / double(q);
        }
        last = now;
    }
    void push(unsigned id, long long now)
    {
        close(now);
        if (depth < MaxDepth)
            stack[depth++] = id < MaxIds ? id : (unsigned)RESIDUAL;
        else
            ++w.overflowDepth;
    }
    // Pops `id`; an unmatched pop unwinds to the most recent `id` (counted). Unknown ids are ignored (counted).
    void pop(unsigned id, long long now)
    {
        close(now);
        if (depth && stack[depth - 1] == id)
        {
            --depth;
            return;
        }
        ++w.mismatches;
        for (unsigned i = depth; i > 0; --i)
            if (stack[i - 1] == id)
            {
                depth = i - 1;
                return;
            }
    }
    void pushList(unsigned type, long long now)
    {
        push(listId(type, eye), now);
    }
    void popList(long long now)
    { // the innermost list scope, whatever its eye tag
        close(now);
        for (unsigned i = depth; i > 0; --i)
            if (isList(stack[i - 1]))
            {
                if (i != depth)
                    ++w.mismatches;
                depth = i - 1;
                return;
            }
        ++w.mismatches;
    }
    // The render eye changes inside a drawlist (SetRenderEye/EndRenderPair are drawlist callbacks): re-tag open lists.
    void setEye(int e, long long now)
    {
        close(now);
        eye = e;
        for (unsigned i = 0; i < depth; ++i)
            if (isList(stack[i]))
            {
                const unsigned type = (stack[i] - ListBase) / EyeSlots;
                stack[i] = listId(type, e);
            }
    }
    // Root boundary: ends the previous root (if any) and starts a new one with an EMPTY stack (a tail scope left open at
    // the end of a loop iteration ends here). Returns true when the window holds `flushAt` roots.
    bool root(long long now, unsigned flushAt)
    {
        close(now);
        bool due = false;
        if (started && q > 0 && now >= rootStart)
        {
            const double period = double(now - rootStart) * 1000.0 / double(q);
            double unnamed = 0, dominantMs = -1;
            unsigned dominant = RESIDUAL;
            for (unsigned id = 0; id < MaxIds; ++id)
            {
                const double v = frame[id];
                if (v <= 0)
                    continue;
                w.cpu[id] += v;
                if (!isNamed(id))
                    unnamed += v;
                if (v > dominantMs)
                {
                    dominantMs = v;
                    dominant = id;
                }
            }
            w.period.add(period);
            w.top.offer(period, dominant, dominantMs);
            ++w.frames;
            w.total += period;
            w.unnamed += unnamed;
            const double frac = period > 0 ? unnamed / period : 0;
            if (frac > w.worstUncovered)
                w.worstUncovered = frac;
            memset(frame, 0, sizeof(frame));
            due = w.frames >= flushAt;
        }
        rootStart = now;
        last = now;
        started = true;
        depth = 0;
        return due;
    }
};
inline double coverage(const Window& w)
{
    return w.total > 0 ? 1.0 - w.unnamed / w.total : 0.0;
}

// GPU: ticks[i] starts the interval charged to f.id[i]; returns false (nothing added) on invalid data.
inline bool addGpuFrame(Window& w, const GpuFrame& f, const unsigned long long* ticks,
                        unsigned long long frequency)
{
    if (!frequency || f.n < 2 || f.n > MaxStamps)
        return false;
    for (unsigned i = 1; i < f.n; ++i)
        if (ticks[i] < ticks[i - 1])
            return false;
    const double k = 1000.0 / double(frequency);
    for (unsigned i = 0; i + 1 < f.n; ++i)
    {
        const unsigned id = f.id[i] < MaxIds ? f.id[i] : (unsigned)RESIDUAL;
        w.gpu[id] += double(ticks[i + 1] - ticks[i]) * k;
    }
    w.gpuSpan += double(ticks[f.n - 1] - ticks[0]) * k;
    ++w.gpuFrames;
    return true;
}

// ---- OpenXR display cadence: consecutive predicted display times (ns). k = round(delta / period).
struct Display
{
    unsigned k[5];
    unsigned n;
    long long last;
    double minDeltaMs; // k[1] on time, k[2] one missed, k[3], k[4] = 3+ missed
    Display() : last(0)
    {
        reset();
    }
    void reset()
    {
        memset(k, 0, sizeof(k));
        n = 0;
        minDeltaMs = 0;
    } // keeps `last`: the cadence continues across windows
    void add(long long displayNs, double periodMs)
    {
        if (displayNs <= 0)
            return;
        if (last > 0 && displayNs > last && periodMs > 0)
        {
            const double d = double(displayNs - last) / 1.0e6;
            unsigned r = (unsigned)(d / periodMs + 0.5);
            if (r < 1)
                r = 1;
            if (r > 4)
                r = 4;
            ++k[r];
            ++n;
            if (minDeltaMs <= 0 || d < minDeltaMs)
                minDeltaMs = d;
        }
        last = displayNs;
    }
};

// ---- TWO_FRAMES eligibility (first failing gate, counted per pair while TWO_FRAMES is requested).
enum TwoReason
{
    TR_ELIGIBLE,
    TR_BRIDGE,
    TR_NOT_WORLD,
    TR_SCOPE_PANEL,
    TR_STOPPED,
    TR_TOGGLE,
    TR_PAUSE,
    TR_VR_MENU,
    TR_PHONE,
    TR_SCRIPT_MENU,
    TR_HOLSTER,
    TR_CROP,
    TR_SCOPE_KEYS,
    TR_OTHER_PATH,
    TWO_REASONS
};
inline const char* twoReasonName(unsigned r)
{
    static const char* const n[TWO_REASONS] = {
        "eligible", "bridge", "not_world",   "scope_panel", "stopped", "toggle",     "pause",
        "vr_menu",  "phone",  "script_menu", "holster",     "crop",    "scope_keys", "other_path"};
    return r < TWO_REASONS ? n[r] : "?";
}

// ---- Formatting (bounded; return chars written or -1).
inline int finish(char* out, size_t size, int n)
{
    if (n < 0 || (size_t)n + 2 > size)
        return -1;
    out[n++] = '\n';
    out[n] = 0;
    return n;
}
// Summary: event=<event> tag=.. frames .. period/p50/p95/p99/max over11 over22 coverage worst_uncovered mismatches ...
inline int formatSummary(const Window& w, const char* event, const char* tag, char* out, size_t size)
{
    if (!out || size < 512 || !w.frames)
        return -1;
    const int n = _snprintf_s(
        out, size, _TRUNCATE,
        "event=%s tag=%s frames=%u period=%.3f p50=%.1f p95=%.1f p99=%.1f max=%.2f over11=%u over22=%u coverage=%.4f worst_uncovered=%.4f mismatches=%u depth_overflow=%u gpu_frames=%u gpu_dropped=%u gpu_disjoint=%u gpu_span=%.3f",
        event, tag ? tag : "-", w.frames, w.period.avg(), w.period.quantile(0.5), w.period.quantile(0.95),
        w.period.quantile(0.99), w.period.peak, w.period.over(11.2), w.period.over(22.3), coverage(w),
        w.worstUncovered, w.mismatches, w.overflowDepth, w.gpuFrames, w.gpuDropped, w.gpuDisjoint,
        w.gpuFrames ? w.gpuSpan / w.gpuFrames : 0.0);
    return finish(out, size, n);
}
// Scopes: " <name>=cpu_ms_per_root[/gpu_ms_per_sampled_frame]" for every nonzero id. Names: fixed table, dl<type>e<slot>,
// cb<slot>.
inline int formatScopes(const Window& w, const char* event, const char* tag, char* out, size_t size)
{
    if (!out || size < 256 || !w.frames)
        return -1;
    int n = _snprintf_s(out, size, _TRUNCATE, "event=%s tag=%s frames=%u", event, tag ? tag : "-", w.frames);
    for (unsigned id = 0; id < MaxIds && n > 0; ++id)
    {
        if (w.cpu[id] <= 0.0 && w.gpu[id] <= 0.0)
            continue;
        char name[32];
        if (id < FIXED_COUNT)
            _snprintf_s(name, sizeof(name), _TRUNCATE, "%s", fixedName(id));
        else if (isList(id))
        {
            static const char* const slot[EyeSlots] = {"shared", "e0", "e1"};
            _snprintf_s(name, sizeof(name), _TRUNCATE, "dl%u_%s", (id - ListBase) / EyeSlots,
                        slot[(id - ListBase) % EyeSlots]);
        }
        else
            _snprintf_s(name, sizeof(name), _TRUNCATE, "cb%u", id - CallbackBase);
        const int k =
            w.gpuFrames && w.gpu[id] > 0.0
                ? _snprintf_s(out + n, size - (size_t)n, _TRUNCATE, " %s=%.3f/%.3f", name,
                              w.cpu[id] / w.frames, w.gpu[id] / w.gpuFrames)
                : _snprintf_s(out + n, size - (size_t)n, _TRUNCATE, " %s=%.3f", name, w.cpu[id] / w.frames);
        n = k < 0 ? -1 : n + k;
    }
    return finish(out, size, n);
}
inline int formatHitches(const Top5& t, const char* event, const char* tag, char* out, size_t size)
{
    if (!out || size < 128)
        return -1;
    int n = _snprintf_s(out, size, _TRUNCATE, "event=%s tag=%s", event, tag ? tag : "-");
    for (unsigned i = 0; i < t.n && n > 0; ++i)
    {
        const int k = _snprintf_s(out + n, size - (size_t)n, _TRUNCATE, " h%u=%.2f/%u/%.2f", i, t.h[i].period,
                                  t.h[i].dominant, t.h[i].dominantMs);
        n = k < 0 ? -1 : n + k;
    }
    return finish(out, size, n);
}
inline int formatXr(const Display& d, const unsigned* two, unsigned pacing, char* out, size_t size)
{
    if (!out || size < 256)
        return -1;
    int n = _snprintf_s(
        out, size, _TRUNCATE,
        "event=PERF146_XR pacing=%u xr_frames=%u xr_min_delta=%.3f xr_k1=%u xr_k2=%u xr_k3=%u xr_k4=%u",
        pacing, d.n, d.minDeltaMs, d.k[1], d.k[2], d.k[3], d.k[4]);
    for (unsigned r = 0; two && r < TWO_REASONS && n > 0; ++r)
    {
        if (!two[r])
            continue;
        const int k =
            _snprintf_s(out + n, size - (size_t)n, _TRUNCATE, " two_%s=%u", twoReasonName(r), two[r]);
        n = k < 0 ? -1 : n + k;
    }
    return finish(out, size, n);
}

} // namespace gx_perf146
#endif // GTAVXR_PERF146_H
