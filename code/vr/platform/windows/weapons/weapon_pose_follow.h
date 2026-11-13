#pragma once
#include "../../../core/hands/calibration.h"
#include "weapon_calibration.h"
namespace gx_weapon_pose_follow
{
struct Local
{
    double v[4][3];
    Local()
    {
        memset(v, 0, sizeof(v));
    }
};
inline bool rigid(const double* m)
{
    if (!m)
        return false;
    for (unsigned c = 0; c < 4; ++c)
        for (unsigned j = 0; j < 3; ++j)
            if (!_finite(m[c * 4 + j]))
                return false;
    for (unsigned c = 0; c < 3; ++c)
        for (unsigned d = 0; d < 3; ++d)
        {
            double dot = 0;
            for (unsigned j = 0; j < 3; ++j)
                dot += m[c * 4 + j] * m[d * 4 + j];
            if (fabs(dot - (c == d ? 1.0 : 0.0)) > .002)
                return false;
        }
    const gx_hands::V a(m[0], m[1], m[2]), b(m[4], m[5], m[6]), c(m[8], m[9], m[10]);
    return fabs(gx_hands::dot(gx_hands::cross(a, b), c) - 1) < .004;
}
inline bool relative(const double* gun, const double* grip, Local& out)
{
    if (!rigid(gun) || !rigid(grip))
        return false;
    for (unsigned c = 0; c < 4; ++c)
        for (unsigned a = 0; a < 3; ++a)
        {
            out.v[c][a] = 0;
            for (unsigned j = 0; j < 3; ++j)
                out.v[c][a] += (gun[c * 4 + j] - (c == 3 ? grip[12 + j] : 0)) * grip[a * 4 + j];
        }
    return true;
}
inline void world(const Local& in, const double* grip, double* out)
{
    memset(out, 0, sizeof(double) * 16);
    out[15] = 1;
    for (unsigned c = 0; c < 4; ++c)
        for (unsigned j = 0; j < 3; ++j)
        {
            out[c * 4 + j] = c == 3 ? grip[12 + j] : 0;
            for (unsigned a = 0; a < 3; ++a)
                out[c * 4 + j] += in.v[c][a] * grip[a * 4 + j];
        }
}
inline Local toAnchor(const Local& gun, const double* anchor)
{
    Local out;
    for (unsigned c = 0; c < 3; ++c)
        for (unsigned j = 0; j < 3; ++j)
            for (unsigned a = 0; a < 3; ++a)
                out.v[c][j] += gun.v[a][j] * anchor[c * 4 + a];
    for (unsigned j = 0; j < 3; ++j)
    {
        out.v[3][j] = gun.v[3][j];
        for (unsigned a = 0; a < 3; ++a)
            out.v[3][j] += gun.v[a][j] * anchor[12 + a];
    }
    return out;
}
inline Local fromAnchor(const Local& basis, const double* anchor)
{
    Local out;
    for (unsigned c = 0; c < 3; ++c)
        for (unsigned j = 0; j < 3; ++j)
            for (unsigned a = 0; a < 3; ++a)
                out.v[c][j] += basis.v[a][j] * anchor[a * 4 + c];
    for (unsigned j = 0; j < 3; ++j)
    {
        out.v[3][j] = basis.v[3][j];
        for (unsigned a = 0; a < 3; ++a)
            out.v[3][j] -= out.v[a][j] * anchor[12 + a];
    }
    return out;
}
inline Local mirrorAnchor(const Local& in, unsigned lateral = 1)
{
    Local out;
    // Native muzzle A is fire direction. Reflect A, C and origin, then flip
    // only the native lateral B column so the same chiral asset stays rigid.
    // No assumption that the gun object's local X is its lateral axis.
    for (unsigned c = 0; c < 4; ++c)
        for (unsigned a = 0; a < 3; ++a)
            out.v[c][a] = in.v[c][a] * (a ? 1.0 : -1.0) * (c == lateral ? -1.0 : 1.0);
    return out;
}
inline unsigned lateralColumn(const double* nativeRight, const double* grip, const double* anchor)
{
    Local relativeRight;
    if (!relative(nativeRight, grip, relativeRight))
        return 1;
    const Local grasp = toAnchor(relativeRight, anchor);
    unsigned axis = 0;
    for (unsigned c = 1; c < 3; ++c)
        if (fabs(grasp.v[c][0]) > fabs(grasp.v[axis][0]))
            axis = c;
    return axis; // authored grasp frame may NOT use the gun muzzle's B lateral convention
}
inline bool calibrate(double* gun, const double* grip, const gx_weapon_calibration::Pose& pose)
{
    int mm[3], degrees[3];
    for (unsigned a = 0; a < 3; ++a)
    {
        mm[a] = pose.value[a];
        degrees[a] = pose.value[3 + a];
    }
    return gx_calibration::weapon(gun, grip, mm, degrees);
}
inline void euler(const Local& basis, double* out)
{
    const double cy = sqrt(basis.v[0][0] * basis.v[0][0] + basis.v[0][1] * basis.v[0][1]);
    out[1] = atan2(-basis.v[0][2], cy);
    if (cy > 1e-9)
    {
        out[0] = atan2(basis.v[1][2], basis.v[2][2]);
        out[2] = atan2(basis.v[0][1], basis.v[0][0]);
    }
    else
    {
        out[0] = atan2(-basis.v[2][1], basis.v[1][1]);
        out[2] = 0;
    }
}
inline void rotate(Local& basis, const double* angles, unsigned columns = 3)
{
    for (unsigned a = 0; a < 3; ++a)
    {
        const double s = sin(angles[a]), k = cos(angles[a]);
        const unsigned u = (a + 1) % 3, v = (a + 2) % 3;
        for (unsigned c = 0; c < columns; ++c)
        {
            const double x = basis.v[c][u], y = basis.v[c][v];
            basis.v[c][u] = k * x - s * y;
            basis.v[c][v] = s * x + k * y;
        }
    }
}
inline Local fromEuler(const double* angles)
{
    Local out;
    for (unsigned a = 0; a < 3; ++a)
        out.v[a][a] = 1;
    rotate(out, angles);
    return out;
}
// Native L/R here are SAME weapon object/attachment, counterfactual anatomy.
// The caller supplies one copied calibration revision, never another live gun.
inline bool apply(const double* nativeL, const double* nativeR, const double* gripL, const double* gripR,
                  const double* anchor, const gx_weapon_calibration::Value& left,
                  const gx_weapon_calibration::Value& right, double* out, unsigned lateral = 1)
{
    if (!out || !nativeL || !gripL || !left.pose.valid() || left.follow >= 64 || lateral > 2)
        return false;
    double explicitL[16];
    memcpy(explicitL, nativeL, sizeof(explicitL));
    if (!calibrate(explicitL, gripL, left.pose))
        return false;
    if (!left.follow)
    {
        memcpy(out, explicitL, sizeof(explicitL));
        return true;
    } // accepted all-explicit path, byte-exact
    if (!right.pose.valid() || !rigid(anchor) || !rigid(nativeR) || !rigid(gripR) || !rigid(nativeL) ||
        !rigid(gripL))
        return false;
    for (unsigned p = 0; p < 6; ++p)
        if ((left.follow & (1u << p)) &&
            left.pose.value[p] != right.pose.value[p] * gx_weapon_calibration::mirrorSign(p))
            return false;
    double calibratedR[16];
    memcpy(calibratedR, nativeR, sizeof(calibratedR));
    if (!calibrate(calibratedR, gripR, right.pose))
        return false;
    Local localExplicit, localRight;
    if (!relative(explicitL, gripL, localExplicit) || !relative(calibratedR, gripR, localRight))
        return false;
    const Local mirroredRight = fromAnchor(mirrorAnchor(toAnchor(localRight, anchor), lateral), anchor);
    Local solved = localExplicit;
    const unsigned angularFollow = left.follow & 56u;
    if (angularFollow == 56u)
    {
        for (unsigned c = 0; c < 3; ++c)
            for (unsigned j = 0; j < 3; ++j)
                solved.v[c][j] = mirroredRight.v[c][j];
    }
    else if (angularFollow)
    {
        // Mixed axes select the native LEFT or reflected RIGHT bind component
        // in the native muzzle frame, then retain existing X->Y->Z hand-axis
        // calibration deltas. No decomposition is written to a profile/menu.
        Local baseL, baseR;
        if (!relative(nativeL, gripL, baseL) || !relative(nativeR, gripR, baseR))
            return false;
        const Local anchorL = toAnchor(baseL, anchor),
                    anchorR = mirrorAnchor(toAnchor(baseR, anchor), lateral);
        double anglesL[3], anglesR[3], angles[3];
        euler(anchorL, anglesL);
        euler(anchorR, anglesR);
        for (unsigned a = 0; a < 3; ++a)
            angles[a] = (angularFollow & (1u << (3 + a))) ? anglesR[a] : anglesL[a];
        Local mixed = fromAnchor(fromEuler(angles), anchor);
        for (unsigned a = 0; a < 3; ++a)
            angles[a] = left.pose.value[3 + a] * 3.14159265358979323846 / 180;
        rotate(mixed, angles);
        for (unsigned c = 0; c < 3; ++c)
            for (unsigned j = 0; j < 3; ++j)
                solved.v[c][j] = mixed.v[c][j];
    }
    for (unsigned a = 0; a < 3; ++a)
        solved.v[3][a] = (left.follow & (1u << a)) ? mirroredRight.v[3][a] : localExplicit.v[3][a];
    double checked[16];
    world(solved, gripL, checked);
    if (!rigid(checked))
        return false;
    memcpy(out, checked, sizeof(checked));
    return true;
}
}
