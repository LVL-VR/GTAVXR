#pragma once
// Private optional optics ABI. Existing GxApi/Input/HMD projection unchanged.
#include "../core/bridge.h"
enum
{
    GX_SCOPE_GEOMETRY_QUALIFIED = 1u,
    GX_SCOPE_ACTIVE = 2u,
    GX_SCOPE_MONOCULAR = 4u
};
// build141 E CONTRACT (frozen by Claude): version 2 + flags QUALIFIED|ACTIVE|MONOCULAR = simplified monocular sniper
 
// The bridge shows ONE head-locked circular image covering GxScopeMonocularDisplayHalfAngle of that eye's view, to that
// eye ONLY, with NO pupil / eye-relief gate (radius/minEyeRelief/maxEyeRelief stay valid but do not gate it); the other
// eye keeps the normal projection. Source = the existing scope crop (EYE_FINAL: still contains the weapon, NOT clean).
// halfAngle = native scope half-FOV (effective zoom); magnification MUST be 1: the display size defines the zoom once
// (displayed angular magnification = tan(GxScopeMonocularDisplayHalfAngle)/tan(halfAngle)). Version 1 is unchanged.
static const double GxScopeMonocularDisplayHalfAngle = 0.40;
#pragma pack(push, 8)
struct GxScopeOptic
{
    uint32_t size, version, hand, flags;
    uint64_t frame, generation;
    // Row-vector WORLD right/up/back/position. Forward is minus back. This is
    // the lens plane, NOT a component root or native camera framing offset.
    double worldLens[16], cameraReference[16];
    GxPose xrReference;
    double radius, halfAngle, magnification, minEyeRelief, maxEyeRelief;
    uint32_t weaponGroup, reserved;
};
struct GxScopeViewing
{
    uint32_t size, version;
    uint64_t frame, generation, tick;
    uint32_t handMask, reserved;
};
#pragma pack(pop)
// Exact native WeaponTypes GROUP_SNIPER hash, not name/attachment heuristics.
static const uint32_t GxNativeSniperGroup = 0xB7BBD827u;
