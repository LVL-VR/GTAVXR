#pragma once
#include <math.h>
#include <float.h>
#include <stdint.h>
 
// ONCE (native sniper camera/zoom/HUD mask+reticle, native weapon hide) and the bridge shows it on a head-locked panel to
// BOTH eyes or ONE eye with the other eye black. Pure POD policy: no engine types, VS2012 compatible.
namespace gx_scope
{
enum View
{
    NATIVE_BOTH = 0,
    NATIVE_ONE = 1,
    CROP = 2,
    OFF = 3,
    VIEW_COUNT = 4
};
// Implementation default (the user has not chosen one): ONE eye, as requested ("QUE EL OTRO OJO NO MUESTRE NADA").
inline int parseView(int v)
{
    return v >= 0 && v < VIEW_COUNT ? v : (int)NATIVE_ONE;
}
inline int nextView(int v)
{
    return (parseView(v) + 1) % VIEW_COUNT;
}
inline bool nativeView(int v)
{
    return v == NATIVE_BOTH || v == NATIVE_ONE;
}
inline const char* viewLabel(int v, bool es)
{
    switch (parseView(v))
    {
    case NATIVE_BOTH:
        return es ? "Vista de mira: NATIVA, ambos ojos" : "Scope view: NATIVE, both eyes";
    case NATIVE_ONE:
        return es ? "Vista de mira: NATIVA, un ojo" : "Scope view: NATIVE, one eye";
    case CROP:
        return es ? "Vista de mira: recorte (con arma)" : "Scope view: crop (shows weapon)";
    default:
        return es ? "Vista de mira: DESACTIVADA" : "Scope view: OFF";
    }
}
// Head-locked panel width in degrees: 60..110, step 10.
inline int parseWidth(int w)
{
    return w >= 60 && w <= 110 && w % 10 == 0 ? w : 80;
}
inline int nextWidth(int w)
{
    const int p = parseWidth(w);
    return p >= 110 ? 60 : p + 10;
}
const double LENS_FILL = 0.98; // build144: lens circle diameter / backbuffer height
const int LENS_SEGMENTS = 96;  // build144: ring tessellation of the black mask outside the lens
const double PANEL_DISTANCE =
    1.0; // metres; apparent size is the angle, distance only matters for the compositor

// Bridge eyes mask: 1 LEFT, 2 RIGHT, 3 BOTH, 0 = do not present (ONE without a resolved eye).
inline unsigned eyesMask(int view, unsigned eye)
{
    if (parseView(view) == NATIVE_BOTH)
        return 3u;
    if (parseView(view) == NATIVE_ONE && eye < 2)
        return 1u << eye;
    return 0u;
}

// Frame-entry gate, frozen at UpdateEntered for the whole update. snapshotAge: updates since the supported-muzzle
// snapshot was taken (1 = taken in the previous update's camera preparation).
struct Gate
{
    bool nativeView, bridgePanel, gameplay, snapshot, sameEpoch;
    unsigned snapshotAge, eyes;
    Gate()
        : nativeView(false), bridgePanel(false), gameplay(false), snapshot(false), sameEpoch(false),
          snapshotAge(0), eyes(0)
    {
    }
};
inline bool enter(const Gate& g)
{
    return g.nativeView && g.bridgePanel && g.gameplay && g.snapshot && g.sameEpoch && g.snapshotAge == 1 &&
           g.eyes >= 1 && g.eyes <= 3;
}

inline bool finite3(const double* v)
{
    return _finite(v[0]) && _finite(v[1]) && _finite(v[2]);
}
inline double dot3(const double* a, const double* b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
inline bool unit3(const double* v, double* out)
{
    if (!finite3(v))
        return false;
    const double n = sqrt(dot3(v, v));
    if (!_finite(n) || n < 1e-6)
        return false;
    out[0] = v[0] / n;
    out[1] = v[1] / n;
    out[2] = v[2] / n;
    return true;
}
// Native camFrame basis (a = right, b = forward, c = up, right-handed: a = b x c) from the muzzle forward axis and a weapon
// "up" hint (the solved gun's up axis); a hint nearly parallel to forward falls back to world up (0,0,1). Roll follows the
// weapon. Returns false for non-finite/degenerate input.
inline bool cameraBasis(const double* forward, const double* upHint, double* right, double* fwd, double* up)
{
    if (!unit3(forward, fwd))
        return false;
    // Fallbacks: world up (0,0,1), then world +Y when aiming straight up/down.
    const double worldUp[3] = {0, 0, 1}, worldY[3] = {0, 1, 0};
    const double* hint = upHint;
    double h[3];
    for (unsigned attempt = 0; attempt < 3; ++attempt)
    {
        if (attempt == 1)
            hint = worldUp;
        else if (attempt == 2)
            hint = worldY;
        if (!finite3(hint))
            continue;
        const double d = dot3(hint, fwd);
        h[0] = hint[0] - fwd[0] * d;
        h[1] = hint[1] - fwd[1] * d;
        h[2] = hint[2] - fwd[2] * d;
        if (unit3(h, up))
        {
            right[0] = fwd[1] * up[2] - fwd[2] * up[1];
            right[1] = fwd[2] * up[0] - fwd[0] * up[2];
            right[2] = fwd[0] * up[1] - fwd[1] * up[0];
            return finite3(right);
        }
    }
    return false;
}
// Camera origin ON the barrel axis (the point of the axis nearest the scope anchor): camera ray and bullet ray are then
// collinear, so the reticle centre matches the muzzle direction at every range. The weapon is hidden natively.
inline bool axisOrigin(const double* muzzle, const double* fwdUnit, const double* anchor, double* out)
{
    if (!finite3(muzzle) || !finite3(fwdUnit) || !finite3(anchor))
        return false;
    const double rel[3] = {anchor[0] - muzzle[0], anchor[1] - muzzle[1], anchor[2] - muzzle[2]};
    const double t = dot3(rel, fwdUnit);
    out[0] = muzzle[0] + fwdUnit[0] * t;
    out[1] = muzzle[1] + fwdUnit[1] * t;
    out[2] = muzzle[2] + fwdUnit[2] * t;
    return finite3(out);
}

// Native vertical-FOV zoom range of the actual scope camera; ONE authority while the scope frame owns stick Y.
// Continuous logarithmic zoom: stick up (positive) zooms in; full deflection crosses the whole range in RANGE_SECONDS.
class Zoom
{
    uint32_t key_;
    double min_, max_, fov_;
    bool variable_;

  public:
    static double RANGE_SECONDS()
    {
        return 1.25;
    }
    static double DEADZONE()
    {
        return 0.2;
    }
    Zoom() : key_(0), min_(0), max_(0), fov_(0), variable_(false) {}
    void reset()
    {
        *this = Zoom();
    }
    // key = camera hash ^ component identity; a changed key/range restarts at the native base FOV.
    bool configure(uint32_t key, double minFov, double maxFov, double baseFov, bool variable)
    {
        if (!key || !_finite(minFov) || !_finite(maxFov) || !_finite(baseFov) || minFov <= 0 ||
            maxFov >= 179 || minFov > maxFov)
        {
            reset();
            return false;
        }
        if (key != key_ || minFov != min_ || maxFov != max_ || variable != variable_)
        {
            key_ = key;
            min_ = minFov;
            max_ = maxFov;
            variable_ = variable;
            fov_ = baseFov < minFov ? minFov : (baseFov > maxFov ? maxFov : baseFov);
        }
        return true;
    }
    bool ready() const
    {
        return key_ != 0 && fov_ >= min_ && fov_ <= max_ && fov_ > 0;
    }
    double fov() const
    {
        return fov_;
    }
    bool update(double stick, double dt)
    {
        if (!ready() || !variable_ || !_finite(stick) || !_finite(dt) || dt <= 0 || dt > 0.25 || max_ <= min_)
            return false;
        if (fabs(stick) < DEADZONE())
            return false;
        const double s = stick > 1 ? 1 : (stick < -1 ? -1 : stick);
        const double span = log(max_ / min_);
        const double next = fov_ * exp(-s * span * dt / RANGE_SECONDS());
        fov_ = next < min_ ? min_ : (next > max_ ? max_ : next);
        return true;
    }
    // Native UI percentage (CUIReticule::GetZoomState): zoom factor Z = maxFov/fov over [1, maxFov/minFov] -> 0..100.
    int percent() const
    {
        if (!ready() || max_ <= min_ + 1e-6)
            return -1;
        const double z = max_ / fov_, zmax = max_ / min_;
        double r = (z - 1) / (zmax - 1);
        r = r < 0 ? 0 : (r > 1 ? 1 : r);
        return (int)floor(r * 100.0);
    }
};
}
