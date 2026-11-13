#pragma once
#include "../../../core/hands/calibration.h"
#include "weapon_pose_follow.h"
namespace gx_weapon_calibration
{
// Change only the weapon's authored support anchor around its PRIMARY grasp.
// Controller targets remain bit-exact. Degrees0 preserves the accepted contact exactly.
inline bool supportOrientation(const double* gun, const double* primary, const int* degrees,
                               gx_hands::V& point)
{
    if (!gun || !primary || !degrees)
        return false;
    for (unsigned a = 0; a < 3; ++a)
        if (degrees[a] < -180 || degrees[a] > 180)
            return false;
    if (!degrees[0] && !degrees[1] && !degrees[2])
        return true;
    if (!_finite(point.x) || !_finite(point.y) || !_finite(point.z))
        return false;
    double pivot[3] = {}, relative[3];
    for (unsigned a = 0; a < 3; ++a)
        for (unsigned j = 0; j < 3; ++j)
        {
            if (!_finite(gun[a * 4 + j]) || !_finite(gun[12 + j]) || !_finite(primary[12 + j]))
                return false;
            pivot[a] += (primary[12 + j] - gun[12 + j]) * gun[a * 4 + j];
        }
    relative[0] = point.x - pivot[0];
    relative[1] = point.y - pivot[1];
    relative[2] = point.z - pivot[2];
    for (unsigned a = 0; a < 3; ++a)
    {
        const double radians = degrees[a] * 3.14159265358979323846 / 180, s = sin(radians), c = cos(radians);
        const unsigned u = (a + 1) % 3, v = (a + 2) % 3;
        const double x = relative[u], y = relative[v];
        relative[u] = c * x - s * y;
        relative[v] = s * x + c * y;
    }
    point = gx_hands::V(pivot[0] + relative[0], pivot[1] + relative[1], pivot[2] + relative[2]);
    return _finite(point.x) && _finite(point.y) && _finite(point.z);
}
// Cache/menu retain their logical mirrored scalars. Only their runtime SUPPORT
// application uses the actual grip->gun bases. Explicit L axes stay gun-local L.
inline bool supportFollowRotation(const int* degrees, unsigned follow)
{
    if (!degrees || follow >= 64)
        return false;
    for (unsigned a = 0; a < 3; ++a)
        if ((follow & (1u << (3 + a))) && degrees[a])
            return true;
    return false;
}
inline bool supportRigidBasis(const double* m)
{
    if (!m)
        return false;
    gx_hands::V axis[3];
    for (unsigned a = 0; a < 3; ++a)
    {
        axis[a] = gx_hands::V(m[a * 4], m[a * 4 + 1], m[a * 4 + 2]);
        if (!_finite(axis[a].x) || !_finite(axis[a].y) || !_finite(axis[a].z) ||
            fabs(gx_hands::dot(axis[a], axis[a]) - 1) > .002)
            return false;
    }
    for (unsigned a = 0; a < 3; ++a)
        if (fabs(gx_hands::dot(axis[a], axis[(a + 1) % 3])) > .002)
            return false;
    return fabs(gx_hands::dot(gx_hands::cross(axis[0], axis[1]), axis[2]) - 1) < .004;
}
inline bool supportOrientationFollow(const double* gunL, const double* gripL, const double* gunR,
                                     const double* gripR, const int* leftDegrees, const int* rightDegrees,
                                     unsigned follow, gx_hands::V& point)
{
    if (!gunL || !gripL || !leftDegrees || !rightDegrees || follow >= 64)
        return false;
    bool any = false, reflected = false;
    for (unsigned a = 0; a < 3; ++a)
    {
        if (leftDegrees[a] < -180 || leftDegrees[a] > 180 || rightDegrees[a] < -180 || rightDegrees[a] > 180)
            return false;
        const bool inherited = (follow & (1u << (3 + a))) != 0;
        const int degrees = inherited ? rightDegrees[a] : leftDegrees[a];
        any = any || degrees != 0;
        reflected = reflected || (inherited && degrees != 0);
    }
    if (!any)
        return true; // exact original authored point, no basis dependency at zero
    if (!reflected)
        return supportOrientation(gunL, gripL, leftDegrees, point);
    if (!supportRigidBasis(gunL) || !supportRigidBasis(gripL) || !supportRigidBasis(gunR) ||
        !supportRigidBasis(gripR) || !_finite(point.x) || !_finite(point.y) || !_finite(point.z))
        return false;
    double bL[3][3] = {}, bR[3][3] = {}, mirror[3][3] = {}, pivot[3] = {}, relative[3];
    for (unsigned row = 0; row < 3; ++row)
        for (unsigned col = 0; col < 3; ++col)
            for (unsigned k = 0; k < 3; ++k)
            {
                bL[row][col] += gripL[row * 4 + k] * gunL[col * 4 + k];
                bR[row][col] += gripR[row * 4 + k] * gunR[col * 4 + k];
            }
    // B_h=R(grip_h)^T R(gun_h); M=B_L^T diag(-1,1,1) B_R.
    for (unsigned row = 0; row < 3; ++row)
        for (unsigned col = 0; col < 3; ++col)
            for (unsigned k = 0; k < 3; ++k)
                mirror[row][col] += bL[k][row] * (k ? 1.0 : -1.0) * bR[k][col];
    for (unsigned a = 0; a < 3; ++a)
        for (unsigned k = 0; k < 3; ++k)
        {
            if (!_finite(gunL[12 + k]) || !_finite(gripL[12 + k]))
                return false;
            pivot[a] += (gripL[12 + k] - gunL[12 + k]) * gunL[a * 4 + k];
        }
    relative[0] = point.x - pivot[0];
    relative[1] = point.y - pivot[1];
    relative[2] = point.z - pivot[2];
    for (unsigned a = 0; a < 3; ++a)
    {
        const bool inherited = (follow & (1u << (3 + a))) != 0;
        const int degrees = inherited ? rightDegrees[a] : leftDegrees[a];
        if (!degrees)
            continue;
        double local[3] = {};
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned col = 0; col < 3; ++col)
                local[row] += (inherited ? mirror[col][row] : (row == col ? 1.0 : 0.0)) * relative[col];
        const double radians = degrees * 3.14159265358979323846 / 180, s = sin(radians), c = cos(radians);
        const unsigned u = (a + 1) % 3, v = (a + 2) % 3;
        const double x = local[u], y = local[v];
        local[u] = c * x - s * y;
        local[v] = s * x + c * y;
        for (unsigned row = 0; row < 3; ++row)
        {
            relative[row] = 0;
            for (unsigned col = 0; col < 3; ++col)
                relative[row] += (inherited ? mirror[row][col] : (row == col ? 1.0 : 0.0)) * local[col];
        }
    }
    const gx_hands::V solved(pivot[0] + relative[0], pivot[1] + relative[1], pivot[2] + relative[2]);
    if (!_finite(solved.x) || !_finite(solved.y) || !_finite(solved.z))
        return false;
    point = solved;
    return true;
}
}
