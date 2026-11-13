#pragma once
 
 
 
// Quest buttons, so menu, recenter, holsters, latch / reload chords and the phone keep their buttons. Every entry SAME (the
 
#include "../xr/locomotion.h"
namespace gx_button_map
{
enum Context
{
    FOOT = 0,
    VEHICLE = 1,
    AIRCRAFT = 2,
    CONTEXT_COUNT = 3
};
enum Source
{
    SRC_A,
    SRC_B,
    SRC_X,
    SRC_Y,
    SRC_LCLICK,
    SRC_RCLICK,
    SOURCE_COUNT
};
enum Target
{
    SAME = 0,
    T_A,
    T_B,
    T_X,
    T_Y,
    T_LB,
    T_RB,
    T_L3,
    T_R3,
    T_BACK,
    T_UP,
    T_DOWN,
    T_LEFT,
    T_RIGHT,
    T_OFF,
    TARGET_COUNT
};
inline unsigned sourceBit(unsigned s)
{
    static const unsigned bits[SOURCE_COUNT] = {GX_A, GX_B, GX_X, GX_Y, GX_LCLICK, GX_RCLICK};
    return s < SOURCE_COUNT ? bits[s] : 0;
}
inline const char* contextKey(unsigned c)
{
    static const char* keys[CONTEXT_COUNT] = {"button_map_foot165", "button_map_vehicle165",
                                              "button_map_aircraft165"};
    return c < CONTEXT_COUNT ? keys[c] : "";
}
inline int sanitize(int t)
{
    return t >= 0 && t < TARGET_COUNT ? t : SAME;
}
// one INI integer per context: 4 bits per source, source 0 lowest; 0 = all SAME (today)
inline int pack(const int* table)
{
    int v = 0;
    for (unsigned s = 0; s < SOURCE_COUNT; ++s)
        v |= sanitize(table[s]) << (4 * s);
    return v;
}
inline void unpack(int v, int* table)
{
    for (unsigned s = 0; s < SOURCE_COUNT; ++s)
        table[s] = v < 0 ? (int)SAME : sanitize((v >> (4 * s)) & 15);
}
inline bool identity(const int* table)
{
    for (unsigned s = 0; s < SOURCE_COUNT; ++s)
        if (table[s] != SAME)
            return false;
    return true;
}
// the pad slot a source occupies today, and the slot of a target
inline int* sourceSlot(gx_loco::Pad& p, unsigned s)
{
    int* slots[SOURCE_COUNT] = {&p.a, &p.b, &p.x, &p.y, &p.ls, &p.rs};
    return s < SOURCE_COUNT ? slots[s] : NULL;
}
inline int* targetSlot(gx_loco::Pad& p, int t)
{
    switch (t)
    {
    case T_A:
        return &p.a;
    case T_B:
        return &p.b;
    case T_X:
        return &p.x;
    case T_Y:
        return &p.y;
    case T_LB:
        return &p.lb;
    case T_RB:
        return &p.rb;
    case T_L3:
        return &p.ls;
    case T_R3:
        return &p.rs;
    case T_BACK:
        return &p.select;
    case T_UP:
        return &p.up;
    case T_DOWN:
        return &p.down;
    case T_LEFT:
        return &p.left;
    case T_RIGHT:
        return &p.right;
    default:
        return NULL; // SAME / OFF
    }
}
// A remapped source leaves its own slot and adds (max) its value into the target slot; values other sources put there stay
// (a grip on LB, a swap A <-> B). OFF drops it.
inline void apply(gx_loco::Pad& p, const int* table)
{
    if (identity(table))
        return;
    int value[SOURCE_COUNT];
    for (unsigned s = 0; s < SOURCE_COUNT; ++s)
        value[s] = *sourceSlot(p, s);
    for (unsigned s = 0; s < SOURCE_COUNT; ++s)
        if (sanitize(table[s]) != SAME)
            *sourceSlot(p, s) = 0;
    for (unsigned s = 0; s < SOURCE_COUNT; ++s)
    {
        int* slot = targetSlot(p, sanitize(table[s]));
        if (slot && value[s] > *slot)
            *slot = value[s];
    }
}
// The context the bindings of this update come from.
inline unsigned context(bool seated, bool aircraftDriver)
{
    return !seated ? FOOT : (aircraftDriver ? AIRCRAFT : VEHICLE);
}
} // namespace gx_button_map
