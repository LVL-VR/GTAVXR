#pragma once
// Pure conservative native skin classifier. Mixed wrist/forearm seams may be
// absent; no torso triangle is admitted merely because one vertex touches a hand.
namespace gx_skin
{
inline bool handVertex(const unsigned* indices, const unsigned* weights, const bool* allowed, unsigned count)
{
    if (!indices || !weights || !allowed || !count || count > 256)
        return false;
    unsigned sum = 0;
    for (unsigned i = 0; i < 4; ++i)
    {
        if (weights[i] > 255)
            return false;
        sum += weights[i];
        if (!weights[i])
            continue;
        if (indices[i] >= count || !allowed[indices[i]])
            return false;
    }
    return sum == 255;
}
}
