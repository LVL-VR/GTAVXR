#pragma once
#include "hand_basis.h"
#include <string.h>
namespace gx_prop
{
inline bool rigid(const double* m)
{
    if (!m)
        return false;
    gx_hands::V axes[3];
    for (unsigned c = 0; c < 3; ++c)
    {
        axes[c] = gx_hands::V(m[c * 4], m[c * 4 + 1], m[c * 4 + 2]);
        if (!_finite(gx_hands::dot(axes[c], axes[c])) || fabs(gx_hands::dot(axes[c], axes[c]) - 1) > .01)
            return false;
    }
    for (unsigned c = 0; c < 3; ++c)
        if (fabs(gx_hands::dot(axes[c], axes[(c + 1) % 3])) > .01)
            return false;
    return gx_hands::dot(gx_hands::cross(axes[0], axes[1]), axes[2]) > .99 && _finite(m[12]) &&
           _finite(m[13]) && _finite(m[14]);
}
// Keep the native animated hand->object grasp; replace ONLY the wrist's spatial
// authority with the real hand. Frame-neutral and model-neutral, no phone Euler.
inline bool attachment(const double* authoredWrist, const double* authoredProp, const double* trackedWrist,
                       double* out)
{
    if (!out || !rigid(authoredWrist) || !rigid(authoredProp) || !rigid(trackedWrist))
        return false;
    double local[4][3];
    for (unsigned c = 0; c < 4; ++c)
        for (unsigned a = 0; a < 3; ++a)
        {
            local[c][a] = 0;
            for (unsigned j = 0; j < 3; ++j)
                local[c][a] += (authoredProp[c * 4 + j] - (c == 3 ? authoredWrist[12 + j] : 0)) *
                               authoredWrist[a * 4 + j];
        }
    if (local[3][0] * local[3][0] + local[3][1] * local[3][1] + local[3][2] * local[3][2] > .3 * .3)
        return false;
    double result[16] = {};
    result[15] = 1;
    for (unsigned c = 0; c < 4; ++c)
        for (unsigned j = 0; j < 3; ++j)
        {
            result[c * 4 + j] = c == 3 ? trackedWrist[12 + j] : 0;
            for (unsigned a = 0; a < 3; ++a)
                result[c * 4 + j] += local[c][a] * trackedWrist[a * 4 + j];
        }
    if (!rigid(result))
        return false;
    memcpy(out, result, sizeof(result));
    return true;
}
}
