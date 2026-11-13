#pragma once
// Versioned transport of native, individually bounded UI pixels. No text, movie
// handles, native resource ownership or STL crosses the v110/OpenXR DLL boundary.
#include "../core/bridge.h"
#include <math.h>
#include <float.h>
#include "hand_ui_routes.h"
#include "cash_ui_policy.h"
#include "../menu/menu_route.h"
enum GxUiRoute
{
    GX_UI_RIGHT_WRIST = 1,
    GX_UI_PANEL = 2,
    GX_UI_HAND_LEFT = gx_hand_routes::Left,
    GX_UI_HAND_RIGHT = gx_hand_routes::Right,
    GX_UI_LEFT_WANTED = 5,
    GX_UI_MENU_PANEL = 6,
    GX_UI_BUTTONS = 7
}; // id1 only on LEFT; native shared menus use a bounded modal carrier
enum
{
    GX_UI_MAX_ITEMS = 48,
    GX_UI_ATLAS = 2048,
    GX_UI_FEED = 0x1000,
    GX_UI_SCRIPT = 0x2000,
    GX_UI_TEXT = 0x3000,
    GX_UI_SPRITES = 0x4000,
    GX_UI_FRONTEND = 0x50000000
};
#pragma pack(push, 8)
struct GxUiItem
{
    uint32_t id, route, priority, x, y, width, height;
};
struct GxUiCapture
{
    uint32_t size, version;
    uint64_t frame, generation;
    uint32_t count, source_width, source_height, reserved;
    GxUiItem items[GX_UI_MAX_ITEMS];
};
#pragma pack(pop)
static_assert(sizeof(GxUiItem) == 28, "UI component ABI");
static_assert(sizeof(GxUiCapture) == 1384, "UI batch ABI");
typedef int(GX_CALL* GxCaptureUi)(void*, const GxUiCapture*, void*);
// Existing export/struct ABI retained. A bounded batch rejection is not an XR
// lifecycle fault; native callers can report zero delivery without shutting VR.
enum GxUiCaptureResult
{
    GX_UI_CAPTURE_OK = 0,
    GX_UI_CAPTURE_FAULT = 1,
    GX_UI_CAPTURE_REJECTED = 2
};
namespace gx_native_ui
{
inline bool handRoute(unsigned route)
{
    return gx_hand_routes::route(route);
}
inline unsigned routeHand(unsigned route)
{
    return gx_hand_routes::hand(route);
}
inline unsigned handId(unsigned hand, gx_hand_context::Owner owner)
{
    return gx_hand_routes::id(hand, owner);
}
inline bool handItem(const GxUiItem& r)
{
    return handRoute(r.route) && gx_hand_routes::valid(r.id, r.route, r.width, r.height);
}
inline bool wantedItem(const GxUiItem& r)
{
    return r.id == 1 && r.route == GX_UI_LEFT_WANTED;
}
inline bool panelRoute(unsigned route)
{
    return route == GX_UI_PANEL || route == GX_UI_MENU_PANEL;
}
// build167 item 3: the EXE-painted wrist buttons card (one fixed identity, its own layer, never a wrist / panel crop)
enum
{
    GX_UI_BUTTONS_ID = 0x70000001,
    GX_UI_BUTTONS_MAX_WIDTH = 1024,
    GX_UI_BUTTONS_MAX_HEIGHT = 512
};
inline bool buttonsItem(const GxUiItem& r)
{
    return r.id == GX_UI_BUTTONS_ID && r.route == GX_UI_BUTTONS && r.width && r.height &&
           r.width <= GX_UI_BUTTONS_MAX_WIDTH && r.height <= GX_UI_BUTTONS_MAX_HEIGHT;
}
inline bool contentDrivenHud(unsigned id)
{
    return id == 1 || id == 10 || id == 11 || id == 12;
}
inline bool cashNeedsDelivery(bool xr, bool force, bool acknowledged, bool active)
{
    return force != acknowledged || (xr && !active);
}
inline bool bounded(const GxUiItem& r, unsigned w, unsigned h)
{
    return r.id && r.priority <= 1000 &&
           (wantedItem(r) || buttonsItem(r) || gx_menyoo_route::belongs(r) ||
            ((r.route == GX_UI_RIGHT_WRIST || panelRoute(r.route))
                 ? r.id != 1 && !gx_hand_routes::contextId(r.id)
                 : handItem(r))) &&
           w && h && w <= 3840 && h <= 2160 && r.width && r.height && r.x < w && r.y < h &&
           r.width <= w - r.x && r.height <= h - r.y;
}
inline bool rectangle(unsigned id, unsigned route, unsigned priority, double x, double y, double w, double h,
                      unsigned sw, unsigned sh, GxUiItem& out)
{
     
    // edges, never accept NaN, zero-size, offscreen or a guessed full-screen HUD.
    if (!_finite(x) || !_finite(y) || !_finite(w) || !_finite(h) || w <= 0 || h <= 0 || x >= 1 || y >= 1 ||
        x + w <= 0 || y + h <= 0 || !sw || !sh || sw > 3840 || sh > 2160)
        return false;
    double left = x < 0 ? 0 : x, top = y < 0 ? 0 : y, right = x + w > 1 ? 1 : x + w,
           bottom = y + h > 1 ? 1 : y + h;
    unsigned l = (unsigned)floor(left * sw), t = (unsigned)floor(top * sh);
    unsigned r = (unsigned)ceil(right * sw), b = (unsigned)ceil(bottom * sh);
    if (r > sw)
        r = sw;
    if (b > sh)
        b = sh;
    out.id = id;
    out.route = route;
    out.priority = priority;
    out.x = l;
    out.y = t;
    out.width = r - l;
    out.height = b - t;
    return bounded(out, sw, sh);
}
inline unsigned hudRoute(unsigned id)
{
     
    // are NEVER sent to either wrist. Script components are sized individually.
    if (!id || id == 8 || id == 14 || id == 16 || id == 18 || id == 19 || id == 20 ||
        (id >= 21 && id < 121) || id > 150)
        return 0;
    if (id == 1)
        return GX_UI_LEFT_WANTED; // native visibility/flash/state, no recreated stars
    if (id == 15)
        return GX_UI_PANEL; // user: subtitles belong on the large notice panel
    return GX_UI_RIGHT_WRIST;
}
inline unsigned priority(unsigned id)
{
    if (id == 3)
        return 100;
    if (id == 1)
        return 95;
    if (id == 2)
        return 90;
    if (id == 10 || id == 11 || id == 12)
        return 85;
    if (id == 15)
        return 80;
    if (id == 13 || id == 5)
        return 75;
    if (id == GX_UI_FEED)
        return 70;
    return 50;
}
inline unsigned fitRoute(const GxUiItem& r)
{
    if (wantedItem(r))
        return GX_UI_LEFT_WANTED;
    if (buttonsItem(r))
        return GX_UI_BUTTONS; // build167 item 3
    if (handRoute(r.route))
        return r.route; // never remap a contextual hand to watch/modal
    // Only a producer with a KNOWN large UI class may request PANEL explicitly.
    // Unknown/general gameplay content never becomes a head panel merely by size.
    return panelRoute(r.route) ? GX_UI_PANEL : GX_UI_RIGHT_WRIST;
}
inline bool gameplayCrop(const GxUiItem& r, unsigned sw, unsigned sh)
{
    return r.route == GX_UI_RIGHT_WRIST && bounded(r, sw, sh) && r.width <= GX_UI_ATLAS - 32 &&
           r.height <= 1000 && (uint64_t)r.width * r.height <= 1300000 &&
           (uint64_t)r.width * r.height * 2 <= (uint64_t)sw * sh;
}
inline bool wantedCrop(const GxUiItem& r, unsigned sw, unsigned sh)
{
    return wantedItem(r) && bounded(r, sw, sh) && r.width <= GX_UI_ATLAS - 32 && r.height <= 1000 &&
           (uint64_t)r.width * r.height <= 1300000 && (uint64_t)r.width * r.height * 2 <= (uint64_t)sw * sh;
}
inline bool contentCrop(const GxUiItem& r, unsigned sw, unsigned sh)
{
    return panelRoute(r.route) ? bounded(r, sw, sh) : gameplayCrop(r, sw, sh) || wantedCrop(r, sw, sh);
}
struct Placement
{
    unsigned x, y, width, height;
    bool placed;
    Placement() : x(0), y(0), width(0), height(0), placed(false) {}
};
class Layout
{
    unsigned nextY, width_;

  public:
    Layout() : nextY(16), width_(0) {}
    // Deterministic priority-sorted cards, equal pixel density and preserved
    // aspect ratio. No original flat X/Y survives in the VR composition.
    Placement append(const GxUiItem& r)
    {
        Placement p;
        if (fitRoute(r) != GX_UI_RIGHT_WRIST || r.width > GX_UI_ATLAS - 32 || r.height > 1000 ||
            (uint64_t)r.width * r.height > 1300000 || nextY > GX_UI_ATLAS - 16 ||
            r.height > GX_UI_ATLAS - nextY - 16)
            return p;
        p.x = 16;
        p.y = nextY;
        p.width = r.width;
        p.height = r.height;
        p.placed = true;
        nextY += r.height + 16;
        if (width_ < r.width + 32)
            width_ = r.width + 32;
        return p;
    }
    unsigned width() const
    {
        return width_;
    }
    unsigned height() const
    {
        return width_ ? nextY : 0;
    }
};
}
