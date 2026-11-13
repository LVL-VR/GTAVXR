#pragma once
// Private optional POD transport, usable by native v110 without D3D headers.
#include <stdint.h>
enum
{
    GX_SCOPE_RETICLE_STRAIGHT = 1u,
    GX_SCOPE_RETICLE_PREMULTIPLIED = 2u
};
#pragma pack(push, 8)
struct GxScopeReticleCapture
{
    uint32_t size, version, reticleHash, alphaMode;
    uint64_t frame, generation;
    uint32_t x, y, width, height;
};
struct GxScopeReticleBinding
{
    uint32_t size, version, hand, reticleHash;
    uint64_t frame, generation;
};
#pragma pack(pop)
