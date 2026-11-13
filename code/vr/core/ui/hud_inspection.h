#pragma once
#include <stddef.h>
namespace gx_hud_observer
{
#if defined(_WIN64)
// Internal source/library API, NOT bridge exports. No GFx SDK header is required
// by the game consumer. Pointers come only from the current native movie scope.
enum CharacterFlags
{
    CharacterVisible = 1,
    CharacterSprite = 2
};
struct CharacterInfo
{
    void* parent;
    const char* url;
    double alpha;
    unsigned children, flags;
};
struct Geometry
{
    double box[4], frame[4];
}; // left/top/width/height, pixel units
static_assert(sizeof(CharacterInfo) == 32, "native x64 opaque metadata size");
static_assert(offsetof(CharacterInfo, alpha) == 16 && offsetof(CharacterInfo, flags) == 28,
              "native x64 opaque metadata fields");
static_assert(sizeof(Geometry) == 64, "native geometry POD size");
void* InspectRoot(void* view);
bool InspectCharacter(void* character, CharacterInfo& out);
const char* InspectInstanceName(void* character);
void* InspectChild(void* character, unsigned index);
bool InspectGeometry(void* view, void* clip, void* root, Geometry& out);
// Sprite drawing-API OWN cached bounds only, never child aggregate or lazy
// ComputeBound/AcquirePath; returns false while native geometry is unqualified.
bool InspectOwnGeometry(void* view, void* clip, void* root, Geometry& out);
#endif
}
