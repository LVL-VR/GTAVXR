#pragma once
#include <float.h>

// CPU-only policies shared by the native seams and the modern/v110 controls.
namespace gx_eye
{
inline bool finite(float v)
{
    return v == v && v <= FLT_MAX && v >= -FLT_MAX;
}
inline bool skipDynamicFacet(bool pairFix, bool dynamic, unsigned visible, unsigned facet)
{
    return dynamic && !pairFix && facet < 32 && (visible & (1u << facet)) == 0;
}
struct WaterSample
{
    float flat, height, depth;
    bool isDefault;
    WaterSample() : flat(0), height(0), depth(0), isDefault(true) {}
    bool underwater() const
    {
        return depth > 0.0f;
    }
};
inline WaterSample waterSample(bool found, float flat, float height, float eyeZ)
{
    WaterSample s;
    if (found && finite(flat) && finite(height) && finite(eyeZ))
    {
        s.flat = flat;
        s.height = height;
        s.isDefault = false;
        const double depth = (double)height - (double)eyeZ;
        if (depth > FLT_MAX)
            return WaterSample();
        s.depth = depth > 0.0 ? (float)depth : 0.0f;
    }
    return s;
}
inline float reflectedShearX(float shear)
{
    return -shear;
}
inline bool flipWaterClip(float normalZ, bool underwater)
{
    return (normalZ < 0.0f) != underwater;
}
inline bool eyeIndex(int eye)
{
    return eye >= 0 && eye < 2;
}

// A shared reflection texture is safe only with producer/consumer adjacency.
// Render callbacks own this ledger; a later UPDATE never changes it.
struct ReflectionOwner
{
    int eye;
    ReflectionOwner() : eye(-1) {}
    void reset()
    {
        eye = -1;
    }
    void produce(int e)
    {
        eye = eyeIndex(e) ? e : -1;
    }
    bool ready(int e) const
    {
        return eyeIndex(e) && eye == e;
    }
};
}
