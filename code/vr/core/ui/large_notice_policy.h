#pragma once
// Read-only native movie identity, not localized text, dimensions or an AS hook.
// Story uses these shared movies too: the MP prefix never authorizes Online.
#include "native_ui.h"
namespace gx_large_notice
{
inline char upper(char c)
{
    return c >= 'a' && c <= 'z' ? char(c - 'a' + 'A') : c;
}
inline bool movie(const char* path, const char* name)
{
    if (!path || !name)
        return false;
    const char* base = path;
    unsigned length = 0;
    for (; length < 256 && path[length]; ++length)
        if (path[length] == '/' || path[length] == '\\')
            base = path + length + 1;
    if (length == 256)
        return false;
    unsigned n = 0;
    while (name[n])
    {
        if (upper(base[n]) != name[n])
            return false;
        ++n;
    }
    if (!base[n])
        return true;
    return (upper(base[n]) == '.' && upper(base[n + 1]) == 'G' && upper(base[n + 2]) == 'F' &&
            upper(base[n + 3]) == 'X' && !base[n + 4]) ||
           (upper(base[n]) == '.' && upper(base[n + 1]) == 'S' && upper(base[n + 2]) == 'W' &&
            upper(base[n + 3]) == 'F' && !base[n + 4]);
}
inline unsigned family(const char* filename)
{
    return movie(filename, "MP_BIG_MESSAGE_FREEMODE") ? 1 : movie(filename, "MIDSIZED_MESSAGE") ? 2 : 0;
}
inline bool method(const char* actual, const char* name)
{
    if (!actual || !name)
        return false;
    unsigned n = 0;
    while (name[n])
    {
        if (upper(actual[n]) != name[n])
            return false;
        ++n;
    }
    return !actual[n];
}
inline bool prefix(const char* actual, const char* name)
{
    if (!actual || !name)
        return false;
    unsigned n = 0;
    while (name[n])
    {
        if (upper(actual[n]) != name[n])
            return false;
        ++n;
    }
    return true;
}
// A method already executed by the original game. -1 is a known animation/query
// with no new message, so it retains the current native presentation class.
inline int effect(const char* name)
{
    const char* panel[] = {"SHOW_WASTED_MP_MESSAGE",
                           "SHOW_BUSTED_MP_MESSAGE",
                           "SHOW_MISSION_END_MP_MESSAGE",
                           "SHOW_MISSION_PASSED_MESSAGE",
                           "SHOW_MISSION_FAILED_MP_MESSAGE",
                           "SHOW_CENTERED_MP_MESSAGE_LARGE",
                           "SHOW_CENTERED_TOP_MP_MESSAGE",
                           "SHOW_SHARD_CENTERED_MP_MESSAGE",
                           "SHOW_SHARD_CENTERED_TOP_MP_MESSAGE",
                           "SHOW_SHARD_WASTED_MP_MESSAGE",
                           "SHOW_SHARD_CENTERED_MP_MESSAGE_LARGE"};
    for (unsigned i = 0; i < sizeof(panel) / sizeof(panel[0]); ++i)
        if (method(name, panel[i]))
            return GX_UI_PANEL;
    if (prefix(name, "TRANSITION_") || prefix(name, "GET_") || prefix(name, "IS_") || prefix(name, "HAS_") ||
        method(name, "SHARD_ANIM_OUT") || method(name, "SET_DISPLAY_CONFIG") ||
        method(name, "UPDATE_MESSAGE"))
        return -1;
    // Includes midsized/rankup/crew rankup/conditional shards, RESET_MOVIE and
    // unknown new presentations: never promote them just because of the file.
    return GX_UI_RIGHT_WRIST;
}
enum
{
    Slots = 128
};
struct Entry
{
    uintptr_t view;
    unsigned family, epoch, route;
    uint64_t serial;
    Entry() : view(0), family(0), epoch(0), route(GX_UI_RIGHT_WRIST), serial(0) {}
};
class Registry
{
    Entry entries_[Slots];
    uint64_t serial_;

  public:
    Registry() : serial_(0) {}
    // Prepared metadata is captured BEFORE the sole native Invoke. POST never
    // accesses a native slot, filename or method that Invoke may have retired.
    void prepared(int id, const void* view, unsigned movieFamily, int update, bool invoked, unsigned before,
                  unsigned after)
    {
        if (id < 0 || id >= Slots || !view || (movieFamily != 1 && movieFamily != 2) || !invoked ||
            before != after)
            return;
        if (update != GX_UI_RIGHT_WRIST && update != GX_UI_PANEL)
            return;
        // Never resurrect an older large class when the latest alias is retired.
        for (unsigned i = 0; i < Slots; ++i)
            if (entries_[i].view == reinterpret_cast<uintptr_t>(view))
                entries_[i].serial = 0;
        Entry& e = entries_[id];
        e.view = reinterpret_cast<uintptr_t>(view);
        e.family = movieFamily;
        e.epoch = after;
        e.route = unsigned(update);
        e.serial = ++serial_;
    }
    void observed(int id, const void* view, unsigned movieFamily, const char* name, bool invoked,
                  unsigned before, unsigned after)
    {
        prepared(id, view, movieFamily, effect(name), invoked, before, after);
    }
    // Latest executed method on this exact GFx view wins, even if native movie
    // slots alias the view. The epoch source invalidates destroy/restart/reuse.
    template <class Epoch> unsigned route(const void* view, unsigned movieFamily, Epoch epoch) const
    {
        if (!view || !movieFamily)
            return GX_UI_RIGHT_WRIST;
        const Entry* latest = 0;
        for (unsigned i = 0; i < Slots; ++i)
        {
            const Entry& e = entries_[i];
            if (e.serial && e.view == reinterpret_cast<uintptr_t>(view) && e.family == movieFamily &&
                e.epoch == epoch(i) && (!latest || e.serial > latest->serial))
                latest = &e;
        }
        return latest ? latest->route : GX_UI_RIGHT_WRIST;
    }
};
inline unsigned route(const char*)
{
    return GX_UI_RIGHT_WRIST;
} // identity alone is deliberately insufficient
}
