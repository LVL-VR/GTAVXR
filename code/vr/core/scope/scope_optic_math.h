#pragma once
#include "scope_optic_packet.h"
#include <math.h>
#include <float.h>
#include <string.h>
namespace gx_scope_optic
{
inline bool finite(double n)
{
    return _finite(n) != 0;
}
inline bool rigid(const double* m)
{
    if (!m)
        return false;
    for (unsigned i = 0; i < 16; ++i)
        if (!finite(m[i]))
            return false;
    if (fabs(m[3]) + fabs(m[7]) + fabs(m[11]) + fabs(m[15] - 1) > 1e-6)
        return false;
    for (unsigned a = 0; a < 3; ++a)
        for (unsigned b = 0; b < 3; ++b)
        {
            double d = 0;
            for (unsigned j = 0; j < 3; ++j)
                d += m[a * 4 + j] * m[b * 4 + j];
            if (fabs(d - (a == b ? 1. : 0.)) > 1e-4)
                return false;
        }
    const double determinant = m[0] * (m[5] * m[10] - m[6] * m[9]) - m[1] * (m[4] * m[10] - m[6] * m[8]) +
                               m[2] * (m[4] * m[9] - m[5] * m[8]);
    return determinant > .9999 && determinant < 1.0001;
}
// build141 E: version 2 monocular packet (see scope_optic_packet.h). Version 1 physical-lens packets are unchanged.
inline bool monocular(const GxScopeOptic& p)
{
    return p.version == 2 &&
           p.flags == (GX_SCOPE_GEOMETRY_QUALIFIED | GX_SCOPE_ACTIVE | GX_SCOPE_MONOCULAR) &&
           (p.reserved == 1 || p.reserved == 2) && p.magnification == 1;
}
inline unsigned monocularEye(const GxScopeOptic& p)
{
    return monocular(p) ? p.reserved - 1 : 2u;
}
inline bool valid(const GxScopeOptic& p, uint64_t frame, uint64_t generation)
{
    const double norm = p.xrReference.qx * p.xrReference.qx + p.xrReference.qy * p.xrReference.qy +
                        p.xrReference.qz * p.xrReference.qz + p.xrReference.qw * p.xrReference.qw;
    const bool physical =
        p.version == 1 && p.flags == (GX_SCOPE_GEOMETRY_QUALIFIED | GX_SCOPE_ACTIVE) && !p.reserved;
    return p.size == sizeof(p) && (physical || monocular(p)) && p.hand < 2 && frame && generation &&
           p.frame == frame && p.generation == generation && p.weaponGroup == GxNativeSniperGroup &&
           rigid(p.worldLens) && rigid(p.cameraReference) && finite(p.xrReference.x) &&
           finite(p.xrReference.y) && finite(p.xrReference.z) && finite(norm) && fabs(norm - 1) < 1e-5 &&
           finite(p.radius) && p.radius >= .0025 && p.radius <= .075 && finite(p.halfAngle) &&
           p.halfAngle > .001 && p.halfAngle < 1.3 && finite(p.magnification) && p.magnification >= 1 &&
           p.magnification <= 16 && finite(p.minEyeRelief) && finite(p.maxEyeRelief) &&
           p.minEyeRelief >= .005 && p.maxEyeRelief >= p.minEyeRelief && p.maxEyeRelief <= .6;
}
// Rigid row-vector matrix -> same physical OpenXR pose. No HMD pose writes.
inline bool pose(const double* m, GxPose& out)
{
    memset(&out, 0, sizeof(out));
    if (!rigid(m))
        return false;
    const double trace = m[0] + m[5] + m[10];
    double s;
    if (trace > 0)
    {
        s = sqrt(trace + 1) * 2;
        out.qw = .25 * s;
        out.qx = (m[6] - m[9]) / s;
        out.qy = (m[8] - m[2]) / s;
        out.qz = (m[1] - m[4]) / s;
    }
    else if (m[0] > m[5] && m[0] > m[10])
    {
        s = sqrt(1 + m[0] - m[5] - m[10]) * 2;
        out.qw = (m[6] - m[9]) / s;
        out.qx = .25 * s;
        out.qy = (m[4] + m[1]) / s;
        out.qz = (m[8] + m[2]) / s;
    }
    else if (m[5] > m[10])
    {
        s = sqrt(1 + m[5] - m[0] - m[10]) * 2;
        out.qw = (m[8] - m[2]) / s;
        out.qx = (m[4] + m[1]) / s;
        out.qy = .25 * s;
        out.qz = (m[9] + m[6]) / s;
    }
    else
    {
        s = sqrt(1 + m[10] - m[0] - m[5]) * 2;
        out.qw = (m[1] - m[4]) / s;
        out.qx = (m[8] + m[2]) / s;
        out.qy = (m[9] + m[6]) / s;
        out.qz = .25 * s;
    }
    const double norm = sqrt(out.qx * out.qx + out.qy * out.qy + out.qz * out.qz + out.qw * out.qw);
    if (!finite(norm) || norm < 1e-6)
        return false;
    out.qx /= norm;
    out.qy /= norm;
    out.qz /= norm;
    out.qw /= norm;
    out.x = m[12];
    out.y = m[13];
    out.z = m[14];
    return true;
}
// Read-only stick-Y zoom ownership; B/reload/left stick and tracking untouched.
class Zoom
{
    double value_;
    bool held_;

  public:
    Zoom() : value_(2), held_(false) {}
    double value() const
    {
        return value_;
    }
    bool set(double n)
    {
        if (!finite(n) || n < 1 || n > 16)
            return false;
        value_ = n;
        return true;
    }
    bool update(bool viewing, double stick)
    {
        if (!viewing || !finite(stick))
        {
            held_ = false;
            return false;
        }
        if (fabs(stick) < .25)
        {
            held_ = false;
            return false;
        }
        if (fabs(stick) < .65 || held_)
            return false;
        held_ = true;
        value_ = stick > 0 ? (value_ >= 8 ? 16 : value_ * 2) : (value_ <= 2 ? 1 : value_ * .5);
        return true;
    }
};
}
