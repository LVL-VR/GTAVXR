#pragma once
// Bounded packing and the COMPLETE owned allocation budget, pure CPU/v110.
// IDs and native crops stay per-element; no text/event dedup or modal fallback.
#include "hud_mirror_policy.h"
#include "native_ui.h"
namespace gx_hud_after
{
enum
{
    AtlasWidth = 2048,
    AtlasHeight = 2048,
    AtlasBytes = AtlasWidth * AtlasHeight * 4
};
inline bool budget(unsigned w, unsigned h, unsigned count)
{
    return gx_hud_mirror::budget(w, h, count) &&
           uint64_t(w) * h * 12 * count + AtlasBytes <= gx_hud_mirror::MaxBytes;
}
struct Pack
{
    GxUiItem source[gx_hud_mirror::MaxTargets], items[gx_hud_mirror::MaxTargets];
    unsigned slot[gx_hud_mirror::MaxTargets], count;
    gx_native_ui::Layout layout;
    bool failed;
    Pack() : count(0), failed(false) {}
    bool add(unsigned index, const GxUiItem& crop, unsigned w, unsigned h)
    {
        if (failed || index >= gx_hud_mirror::MaxTargets || count == gx_hud_mirror::MaxTargets ||
            !(gx_native_ui::gameplayCrop(crop, w, h) || gx_native_ui::wantedCrop(crop, w, h)))
        {
            failed = true;
            return false;
        }
        for (unsigned i = 0; i < count; ++i)
            if (slot[i] == index || items[i].id == crop.id)
            {
                failed = true;
                return false;
            }
        // The mirror atlas is storage, not a wrist presentation. Reserve wanted
        // pixels with the SAME bounded packing, retain its exclusive route in POD.
        GxUiItem storage = crop;
        if (gx_native_ui::wantedItem(storage))
            storage.route = GX_UI_RIGHT_WRIST;
        const gx_native_ui::Placement p = layout.append(storage);
        if (!p.placed)
        {
            failed = true;
            return false;
        }
        GxUiItem item = crop;
        item.x = p.x;
        item.y = p.y;
        if (!gx_native_ui::bounded(item, AtlasWidth, AtlasHeight))
        {
            failed = true;
            return false;
        }
        source[count] = crop;
        items[count] = item;
        slot[count++] = index;
        return true;
    }
    bool ready() const
    {
        return !failed && count && count <= gx_hud_mirror::MaxTargets;
    }
};
}
