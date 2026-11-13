#pragma once
#include "scope_panel_api.h"
#include <float.h>
#include <math.h>

namespace gx_scope_panel {
inline bool valid(const GxScopePanel& p) {
    return p.size == sizeof(p) && p.version == 1 && p.eyes >= 1 && p.eyes <= 3 && !p.reserved &&
        _finite(p.width_degrees) && p.width_degrees >= 20 && p.width_degrees <= 120 &&
        _finite(p.distance_m) && p.distance_m >= .5 && p.distance_m <= 5;
}
// OpenXR XrEyeVisibility values, kept independent of runtime headers for v110.
enum Visibility { Both = 0, Left = 1, Right = 2 };
inline Visibility visibility(uint32_t eyes) { return eyes == 1 ? Left : eyes == 2 ? Right : Both; }
struct Size {
    double width, height;
    Size() : width(0), height(0) {}
};
inline bool size(const GxScopePanel& p, uint32_t w, uint32_t h, Size& out) {
    if (!valid(p) || !w || !h) return false;
    Size next;
    next.width = 2 * p.distance_m * tan(p.width_degrees * (3.14159265358979323846 / 360));
    next.height = next.width * double(h) / double(w);
    out = next;
    return true;
}
struct Frame {
    bool active;
    GxScopePanel request;
    Frame() : active(false), request() {}
    void reset() { active = false; request = GxScopePanel(); }
    bool set(bool begun, uint32_t copied, bool menu, const GxScopePanel* p) {
        if (!begun || copied || menu || !p || !valid(*p)) return false;
        request = *p;
        active = true;
        return true;
    }
};
}

