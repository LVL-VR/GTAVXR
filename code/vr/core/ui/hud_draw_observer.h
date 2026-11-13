#pragma once
#include "hud_mirror_policy.h"
 
// Installed by one render-thread MovieScope; stock execution stays original-once.
// Clip events do NOT write rendererString, AS properties, visibility or userdata.
namespace gx_hud_observer
{
enum Trait
{
    Filtered = 1,
    ThreeDimensional = 2,
    DisplayCallback = 4,
    NonNormalBlend = 8
};
enum DrawKind
{
    Vertices = 1,
    Indexed = 2
};
struct Hooks
{
    void* user;
    void (*beforeMovie)(void*, int);
    void (*clip)(void*, const void*, const void*, const char*, bool, unsigned);
    void (*pass)(void*, bool, bool);
    void (*draw)(void*, void*, unsigned, unsigned, unsigned, int);
    void (*clear)(void*, void*, unsigned, const float*, float, unsigned);
    bool empty() const
    {
        return !user && !beforeMovie && !clip && !pass && !draw && !clear;
    }
};
#if defined(_WIN64)
extern __declspec(thread) Hooks current;
extern __declspec(thread) gx_hud_mirror::QueryLedger counterQueries;
#endif
}
