#pragma once
// build133: runtime-driven per-eye render size. Shared verbatim by the bridge DLL (modern MSVC) and the
// engine adapter (v110). The runtime's recommended per-view size (Virtual Desktop preset) times a user scale
 
// runtime FOV, so the image size changes sharpness/cost only, never geometry.
#include <stdint.h>
enum GxEyeSizeMode
{
    GX_EYE_NATIVE = 0,
    GX_EYE_AUTO = 1,
    GX_EYE_MANUAL = 2
};
enum GxEyeSizeStatus
{
    GX_EYE_OK = 0,
    GX_EYE_ABI = 1,
    GX_EYE_LOADER = 2,
    GX_EYE_RUNTIME = 3,
    GX_EYE_VIEWS = 4,
    GX_EYE_POLICY = 5,
    GX_EYE_EXCEPTION = 6
};
enum GxEyeSizeClamp
{
    GX_EYE_CLAMP_SCALE = 1,
    GX_EYE_CLAMP_MAX = 2,
    GX_EYE_CLAMP_MIN = 4
};
struct GxEyeSizeRequest
{
    uint32_t size, version, mode, scale_percent, manual_width, manual_height;
};
struct GxEyeSizeResult
{
    uint32_t size, version, status, mode, scale_percent;
    uint32_t recommended_width, recommended_height, max_width, max_height;
    uint32_t width, height, clamped;
    char runtime[64];
};
namespace gx_eye_size
{
enum
{
    MIN_SCALE = 25,
    MAX_SCALE = 200,
    MIN_SIDE = 256,
    MAX_SIDE = 4096,
    ALIGN = 8
};
inline uint32_t clampScale(uint32_t s)
{
    return s < (uint32_t)MIN_SCALE ? (uint32_t)MIN_SCALE
                                   : (s > (uint32_t)MAX_SCALE ? (uint32_t)MAX_SCALE : s);
}
// AUTO uses the runtime recommendation, MANUAL a fixed size; NATIVE (window-sized eyes) is not chosen here.
// The result keeps the base aspect, never exceeds the runtime maximum or MAX_SIDE, is at least MIN_SIDE on
// its shorter side and is floored to ALIGN pixels. Returns false when no valid size exists.
inline bool choose(uint32_t mode, uint32_t recW, uint32_t recH, uint32_t maxW, uint32_t maxH, uint32_t scale,
                   uint32_t manW, uint32_t manH, uint32_t& outW, uint32_t& outH, uint32_t& clamped)
{
    outW = outH = 0;
    clamped = 0;
    uint32_t baseW = 0, baseH = 0;
    if (mode == (uint32_t)GX_EYE_AUTO)
    {
        baseW = recW;
        baseH = recH;
    }
    else if (mode == (uint32_t)GX_EYE_MANUAL)
    {
        baseW = manW;
        baseH = manH;
    }
    else
        return false;
    if (!baseW || !baseH || !maxW || !maxH || baseW > 65536u || baseH > 65536u)
        return false;
    const uint32_t s = clampScale(scale);
    if (s != scale)
        clamped |= GX_EYE_CLAMP_SCALE;
    double w = double(baseW) * double(s) / 100.0, h = double(baseH) * double(s) / 100.0;
    const double limitW = double(maxW < (uint32_t)MAX_SIDE ? maxW : (uint32_t)MAX_SIDE);
    const double limitH = double(maxH < (uint32_t)MAX_SIDE ? maxH : (uint32_t)MAX_SIDE);
    if (limitW < double(MIN_SIDE) || limitH < double(MIN_SIDE))
        return false;
    double f = 1.0;
    if (w > limitW)
        f = limitW / w;
    if (h * f > limitH)
        f = limitH / h;
    if (f < 1.0)
    {
        w *= f;
        h *= f;
        clamped |= GX_EYE_CLAMP_MAX;
    }
    const double shorter = w < h ? w : h;
    if (shorter < double(MIN_SIDE))
    {
        const double g = double(MIN_SIDE) / shorter;
        w *= g;
        h *= g;
        clamped |= GX_EYE_CLAMP_MIN;
        if (w > limitW + 0.5 || h > limitH + 0.5)
            return false;
    }
    uint32_t iw = uint32_t(w + 1e-6) / (uint32_t)ALIGN * (uint32_t)ALIGN,
             ih = uint32_t(h + 1e-6) / (uint32_t)ALIGN * (uint32_t)ALIGN;
    if (iw < (uint32_t)MIN_SIDE)
        iw = (uint32_t)MIN_SIDE;
    if (ih < (uint32_t)MIN_SIDE)
        ih = (uint32_t)MIN_SIDE;
    if (double(iw) > limitW || double(ih) > limitH)
        return false;
    outW = iw;
    outH = ih;
    return true;
}
// Settings text -> mode. Unknown text keeps the safe window-sized route.
inline uint32_t parseMode(const char* text)
{
    if (!text)
        return GX_EYE_NATIVE;
    char v[16] = {0};
    unsigned n = 0;
    for (; text[n] && n < 15; ++n)
    {
        char c = text[n];
        v[n] = (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
    }
    if (!v[0])
        return GX_EYE_AUTO;
    const char* autoText = "auto";
    const char* manualText = "manual";
    bool isAuto = true, isManual = true;
    for (unsigned i = 0; i < 16; ++i)
    {
        if (v[i] != autoText[i])
        {
            isAuto = false;
        }
        if (!autoText[i])
            break;
    }
    for (unsigned i = 0; i < 16; ++i)
    {
        if (v[i] != manualText[i])
        {
            isManual = false;
        }
        if (!manualText[i])
            break;
    }
    return isAuto ? (uint32_t)GX_EYE_AUTO : (isManual ? (uint32_t)GX_EYE_MANUAL : (uint32_t)GX_EYE_NATIVE);
}
}
