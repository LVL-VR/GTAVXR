#pragma once
#include <string.h>
#include "hand_basis.h"
namespace gx_calibration
{
inline int baseMm(unsigned hand, unsigned axis)
{
    return axis == 0 ? (hand ? 40 : -40) : (axis == 1 ? 40 : 8);
}
inline int basePitch()
{
    return 20;
}
inline int legacyDelta(int absolute, unsigned hand, unsigned axis)
{
    return absolute - baseMm(hand, axis);
}
inline bool hand(double* m, unsigned h, const int* delta, int pitch)
{
    if (h > 1 || !delta || pitch < -50 || pitch > 50)
        return false;
    int physical[3];
    for (unsigned c = 0; c < 3; ++c)
    {
        if (delta[c] < -150 || delta[c] > 150)
            return false;
        physical[c] = baseMm(h, c) + delta[c];
    }
    // Logical deltas are bounded separately; the accepted baseline may bring
    // the total above the original150mm debug slider extent.
    int zero[3] = {0, 0, 0};
    if (!gx_hands::translateGrip(m, zero))
        return false;
    for (unsigned c = 0; c < 3; ++c)
    {
        for (unsigned j = 0; j < 3; ++j)
            m[12 + j] += m[c * 4 + j] * physical[c] * .001;
    }
    return true;
}
// Rotate a complete attachment about the physical hand pivot in hand-local axes.
// Translation is applied afterward in those same axes. No left-hand target is changed.
inline bool weapon(double* gun, const double* hand, const int* mm, const int* degrees)
{
    if (!gun || !hand || !mm || !degrees)
        return false;
    double local[4][3];
    for (unsigned c = 0; c < 4; ++c)
        for (unsigned a = 0; a < 3; ++a)
        {
            local[c][a] = 0;
            for (unsigned j = 0; j < 3; ++j)
            {
                const double v = gun[c * 4 + j] - (c == 3 ? hand[12 + j] : 0);
                if (!_finite(v) || !_finite(hand[a * 4 + j]))
                    return false;
                local[c][a] += v * hand[a * 4 + j];
            }
        }
    for (unsigned a = 0; a < 3; ++a)
    {
        if (mm[a] < -150 || mm[a] > 150 || degrees[a] < -180 || degrees[a] > 180)
            return false;
        const double t = degrees[a] * 3.14159265358979323846 / 180, s = sin(t), k = cos(t);
        const unsigned u = (a + 1) % 3, v = (a + 2) % 3;
        for (unsigned c = 0; c < 4; ++c)
        {
            double x = local[c][u], y = local[c][v];
            local[c][u] = k * x - s * y;
            local[c][v] = s * x + k * y;
        }
    }
    for (unsigned a = 0; a < 3; ++a)
        local[3][a] += mm[a] * .001;
    for (unsigned c = 0; c < 4; ++c)
        for (unsigned j = 0; j < 3; ++j)
        {
            gun[c * 4 + j] = c == 3 ? hand[12 + j] : 0;
            for (unsigned a = 0; a < 3; ++a)
                gun[c * 4 + j] += local[c][a] * hand[a * 4 + j];
        }
    return true;
}
// Move only the requested support POINT in left grip-local space. The weapon
// solver rotates about its right grasp toward this point; neither tracker moves.
inline bool supportPoint(const double* hand, const int* mm, gx_hands::V& point)
{
    if (!hand || !mm)
        return false;
    int zero[3] = {};
    double checked[16];
    memcpy(checked, hand, sizeof(checked));
    if (!gx_hands::translateGrip(checked, zero))
        return false;
    double p[3] = {hand[12], hand[13], hand[14]};
    for (unsigned c = 0; c < 3; ++c)
    {
        if (mm[c] < -150 || mm[c] > 150)
            return false;
        for (unsigned j = 0; j < 3; ++j)
            p[j] += hand[c * 4 + j] * mm[c] * .001;
    }
    point = gx_hands::V(p[0], p[1], p[2]);
    return true;
}
}
