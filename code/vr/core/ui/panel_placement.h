#pragma once
 
// EN LAS MUNECAS ASI PUEDO PONERLO EN EL TABLERO O ENCIMA DE LA RADIO COMO GPS"): where each HUD panel is drawn. Shared by the
 
// DEFAULT = the panel's own placement of today (wrist profiles, element profiles); any other anchor replaces only its pose:
// pose = anchor pose (OpenXR LOCAL space) * offset, size * scale; face = the quad turns its front to the head.
// VEHICLE = a pose in LOCAL space the EXE computes every frame (the player's seat frame in the vehicle).
 
#include "../core/bridge.h"
#include "wrist_calibration.h"
#include <math.h>
#include <string.h>
#pragma pack(push, 8)
struct GxPanelPlace
{
    uint32_t anchor, face;
    GxPose offset;
    float scale, reserved;
};
struct GxPanelPlacements
{
    uint32_t size, version, vehicleValid, reserved;
    GxPose vehicle;
    GxPanelPlace panel[11];
};
#pragma pack(pop)
static_assert(sizeof(GxPanelPlace) == 72, "panel place POD ABI");
static_assert(sizeof(GxPanelPlacements) == 864, "panel placements POD ABI");
typedef int(GX_CALL* GxSetPanelPlacements)(void*, const GxPanelPlacements*);
namespace gx_panel
{
enum Panel
{
    MAP,
    WANTED,
    RIGHT,
    EL_MONEY,
    EL_NOTIFICATIONS,
    EL_PHONE,
    EL_HELP,
    EL_LOCATION,
    EL_STATUS,
    EL_OTHER,
    BUTTONS,
    COUNT
};
enum Anchor
{
    DEFAULT,
    LEFT_HAND,
    RIGHT_HAND,
    HEAD,
    ROOM,
    VEHICLE,
    ANCHOR_COUNT
};
// gx_ui_element kind (Money 0, Notifications 1, Phone 2, Help 3, Subtitles 4, Location 5, Status 6, Other 7, Wanted 8) -> panel;
// subtitles stay the front panel of today (COUNT = not movable)
inline unsigned elementPanel(unsigned kind)
{
    static const unsigned map[9] = {EL_MONEY,    EL_NOTIFICATIONS, EL_PHONE, EL_HELP, COUNT,
                                    EL_LOCATION, EL_STATUS,        EL_OTHER, WANTED};
    return kind < 9 ? map[kind] : COUNT;
}
inline GxPose identity()
{
    GxPose p = {0, 0, 0, 0, 0, 0, 1};
    return p;
}
inline GxPanelPlace place()
{
    GxPanelPlace e;
    memset(&e, 0, sizeof(e));
    e.anchor = DEFAULT;
    e.offset = identity();
    e.scale = 1.f;
    return e;
}
inline GxPanelPlacements defaults()
{
    GxPanelPlacements p;
    memset(&p, 0, sizeof(p));
    p.size = sizeof(p);
    p.version = 2;
    p.vehicle = identity();
    for (unsigned k = 0; k < COUNT; ++k)
        p.panel[k] = place();
    return p;
}
inline bool finitePose(const GxPose& p, double reach)
{
    const double v[7] = {p.x, p.y, p.z, p.qx, p.qy, p.qz, p.qw};
    for (unsigned i = 0; i < 7; ++i)
        if (!(v[i] == v[i]) || v[i] > 1e6 || v[i] < -1e6)
            return false;
    const double n = p.qx * p.qx + p.qy * p.qy + p.qz * p.qz + p.qw * p.qw;
    return fabs(n - 1) < .01 && fabs(p.x) <= reach && fabs(p.y) <= reach && fabs(p.z) <= reach;
}
inline bool valid(const GxPanelPlacements& p)
{
    if (p.size != sizeof(p) || p.version != 2 || p.vehicleValid > 1 || !finitePose(p.vehicle, 1e5))
        return false;
    for (unsigned k = 0; k < COUNT; ++k)
    {
        const GxPanelPlace& e = p.panel[k];
        if (e.anchor >= ANCHOR_COUNT || e.face > 1 || !(e.scale >= .3f && e.scale <= 3.f) ||
            !finitePose(e.offset, 5))
            return false;
    }
    return true;
}
inline gx_wrist::Quaternion quat(const GxPose& p)
{
    return gx_wrist::Quaternion(p.qx, p.qy, p.qz, p.qw);
}
inline gx_wrist::Quaternion conj(const gx_wrist::Quaternion& q)
{
    return gx_wrist::Quaternion(-q.x, -q.y, -q.z, q.w);
}
inline void rotate(const gx_wrist::Quaternion& q, const double* v, double* out)
{
    const gx_wrist::Quaternion r =
        gx_wrist::multiply(gx_wrist::multiply(q, gx_wrist::Quaternion(v[0], v[1], v[2], 0)), conj(q));
    out[0] = r.x;
    out[1] = r.y;
    out[2] = r.z;
}
inline GxPose normalized(GxPose p)
{
    const double n = sqrt(p.qx * p.qx + p.qy * p.qy + p.qz * p.qz + p.qw * p.qw);
    if (n > 1e-9)
    {
        p.qx /= n;
        p.qy /= n;
        p.qz /= n;
        p.qw /= n;
    }
    else
    {
        p.qx = p.qy = p.qz = 0;
        p.qw = 1;
    }
    return p;
}
// build167 item 3: the buttons card width (metres, before the wrist / panel scale) and its DEFAULT pose: beside the map
// (map quad frame: x right; the map is .28 m * wrist scale wide), so the EXE pokes exactly what the bridge draws.
const double BUTTONS_WIDTH = .13;
// anchor * offset
inline GxPose compose(const GxPose& anchor, const GxPose& offset)
{
    const gx_wrist::Quaternion a = quat(anchor);
    const double t[3] = {offset.x, offset.y, offset.z};
    double r[3];
    rotate(a, t, r);
    const gx_wrist::Quaternion q = gx_wrist::multiply(a, quat(offset));
    GxPose out = {anchor.x + r[0], anchor.y + r[1], anchor.z + r[2], q.x, q.y, q.z, q.w};
    return normalized(out);
}
// the offset that puts a panel at `world` on `anchor` (inverse of compose)
inline GxPose relative(const GxPose& anchor, const GxPose& world)
{
    const gx_wrist::Quaternion ai = conj(quat(anchor));
    const double d[3] = {world.x - anchor.x, world.y - anchor.y, world.z - anchor.z};
    double r[3];
    rotate(ai, d, r);
    const gx_wrist::Quaternion q = gx_wrist::multiply(ai, quat(world));
    GxPose out = {r[0], r[1], r[2], q.x, q.y, q.z, q.w};
    return normalized(out);
}
inline GxPose buttonsBesideMap(const GxPose& map, double wristScale)
{
    const GxPose o = {(.14 + BUTTONS_WIDTH / 2 + .01) * wristScale, 0, .004, 0, 0, 0, 1};
    return compose(map, o);
}
// The quad's front (+Z) towards the head, its up as close to LOCAL +Y as possible (readable, no roll).
inline GxPose faceHead(const GxPose& panel, const GxPose& head)
{
    double f[3] = {head.x - panel.x, head.y - panel.y, head.z - panel.z};
    const double n = sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (!(n > 1e-4))
        return panel;
    for (unsigned c = 0; c < 3; ++c)
        f[c] /= n;
    double up[3] = {0, 1, 0};
    if (fabs(f[1]) > .98)
    {
        const double fwd[3] = {0, 0, -1};
        double tmp[3];
        rotate(quat(panel), fwd, tmp);
        up[0] = tmp[0];
        up[1] = 0;
        up[2] = tmp[2];
    }
    double x[3] = {up[1] * f[2] - up[2] * f[1], up[2] * f[0] - up[0] * f[2], up[0] * f[1] - up[1] * f[0]};
    const double xn = sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
    if (!(xn > 1e-6))
        return panel;
    for (unsigned c = 0; c < 3; ++c)
        x[c] /= xn;
    const double y[3] = {f[1] * x[2] - f[2] * x[1], f[2] * x[0] - f[0] * x[2], f[0] * x[1] - f[1] * x[0]};
    // rotation matrix columns x, y, f(=z) -> quaternion
    const double m00 = x[0], m11 = y[1], m22 = f[2], tr = m00 + m11 + m22;
    GxPose out = panel;
    if (tr > 0)
    {
        const double s = sqrt(tr + 1) * 2;
        out.qw = .25 * s;
        out.qx = (y[2] - f[1]) / s;
        out.qy = (f[0] - x[2]) / s;
        out.qz = (x[1] - y[0]) / s;
    }
    else if (m00 > m11 && m00 > m22)
    {
        const double s = sqrt(1 + m00 - m11 - m22) * 2;
        out.qw = (y[2] - f[1]) / s;
        out.qx = .25 * s;
        out.qy = (y[0] + x[1]) / s;
        out.qz = (f[0] + x[2]) / s;
    }
    else if (m11 > m22)
    {
        const double s = sqrt(1 + m11 - m00 - m22) * 2;
        out.qw = (f[0] - x[2]) / s;
        out.qx = (y[0] + x[1]) / s;
        out.qy = .25 * s;
        out.qz = (f[1] + y[2]) / s;
    }
    else
    {
        const double s = sqrt(1 + m22 - m00 - m11) * 2;
        out.qw = (x[1] - y[0]) / s;
        out.qx = (f[0] + x[2]) / s;
        out.qy = (f[1] + y[2]) / s;
        out.qz = .25 * s;
    }
    return normalized(out);
}
} // namespace gx_panel
