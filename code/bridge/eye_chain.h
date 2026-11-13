#pragma once
 
// EXE (VS2012 v110) and the bridge (modern MSVC) plus pure policy: SR ratios, render size, Halton jitter and the column-major
 
#include <stdint.h>
#include <math.h>
#ifndef GX_CALL
#ifdef _WIN32
#define GX_CALL __cdecl
#else
#define GX_CALL
#endif
#endif

#pragma pack(push, 8)
 
struct GxEyeChainConfig {
    uint32_t size, version;                       // sizeof, 1
    uint32_t render_width, render_height;          
    uint32_t output_width, output_height;         // swapchain size (= GxConfig)
    uint32_t upscale, neural, neural_intensity;   // gx_eye_chain::Upscale, 0/1, 0..100
    uint32_t reserved;
    const wchar_t* dlss_dir;                      // the player's DLSS model folder (SR model, optionally NR); the Suite finds it
    const wchar_t* forwarder_dir;                 // folder with our two NGX forwarder copies (the bridge folder)
};
// One eye of one world pair, queued by the update thread, handed over on the render thread just before capture.
// CAMERA-RELATIVE: "rel" = world position minus this eye's camera position in THIS pair. Both current and previous composites
// use that same origin, so depth -> rel -> previous clip keeps float precision thousands of metres from the map origin.
struct GxEyeGuides {
    uint32_t size, version, eye, reset;           // sizeof, 1, 0/1, 1 = history invalid (cut, first, gap)
    uint64_t frame, generation;
    float clip_from_rel[16];                      // Projection * View * Translate(camera) of this eye, WITHOUT jitter (column-major)
    float rel_from_clip[16];                      // its inverse
    float prev_clip_from_rel[16];                 // the same eye's previous pair, same origin (= clip_from_rel when reset)
    float jitter_px[2];                           // measured image shift of the jitter, render pixels, +x right, +y down
    uint32_t render_width, render_height;
};
struct GxEyeChainStatus {
    uint32_t size, version;
    uint32_t state;                               // gx_eye_chain::State
    uint32_t neural_active, upscale, compare;
    uint32_t fault_stage, fault_code;
    float ms_capture, ms_neural, ms_upscale, ms_return;   // GPU ms per eye, rolling 120-frame mean (developer mode readings)
    char reason[64];
};
#pragma pack(pop)
typedef int (GX_CALL *GxConfigureEyeChain)(void*, const GxEyeChainConfig*);
typedef int (GX_CALL *GxCaptureEyeGuides)(void*, const GxEyeGuides*, void* d3d_depth_texture);
typedef int (GX_CALL *GxSetNeuralParams)(void*, uint32_t enabled, uint32_t intensity, uint32_t compare);
typedef int (GX_CALL *GxReadEyeChainStatus)(void*, GxEyeChainStatus*);

namespace gx_eye_chain {

enum Upscale {OFF=0, QUALITY=1, BALANCED=2, PERFORMANCE=3, ULTRA=4, UPSCALE_COUNT=5};
enum State {NATIVE=0, CHAIN=1, BLIT_FALLBACK=2};
enum Compare {COMPARE_OFF=0, COMPARE_SPLIT=1};

inline char Lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c; }
inline bool Same(const char* a, const char* b)
{
    if(!a || !b) return false;
    while(*a && *b) { if(Lower(*a) != Lower(*b)) return false; ++a; ++b; }
    return *a == *b;
}
// vr-settings.ini [Graphics] upscale= off | quality | balanced | performance | ultra (anything else = off)
inline uint32_t ParseUpscale(const char* s)
{
    return Same(s, "quality") ? QUALITY : Same(s, "balanced") ? BALANCED : Same(s, "performance") ? PERFORMANCE : Same(s, "ultra") ? ULTRA : OFF;
}
// DLSS SR scale factors per axis (Quality 1/1.5, Balanced 1/1.724, Performance 1/2, Ultra performance 1/3)
inline double Ratio(uint32_t u)
{
    return u == QUALITY ? 2.0 / 3.0 : u == BALANCED ? 0.58 : u == PERFORMANCE ? 0.5 : u == ULTRA ? 1.0 / 3.0 : 1.0;
}
// render size of one eye: output * ratio, floored to 8, at least 256 on each side, never above the output
inline bool RenderSize(uint32_t outW, uint32_t outH, uint32_t u, uint32_t& w, uint32_t& h)
{
    w = h = 0;
    if(!outW || !outH || u >= UPSCALE_COUNT) return false;
    const double r = Ratio(u);
    uint32_t rw = (uint32_t)floor(outW * r / 8.0) * 8u, rh = (uint32_t)floor(outH * r / 8.0) * 8u;
    if(rw < 256u) rw = 256u;
    if(rh < 256u) rh = 256u;
    if(rw > outW) rw = outW;
    if(rh > outH) rh = outH;
    w = rw; h = rh;
    return true;
}
inline float Halton(uint32_t index, uint32_t base)
{
    float f = 1.f, r = 0.f;
    while(index) { f /= (float)base; r += f * (float)(index % base); index /= base; }
    return r;
}
// requested sub-pixel jitter of a world pair, render pixels in (-0.5, 0.5); both eyes share the phase (separate SR history)
inline void Jitter(uint64_t pair, uint32_t phases, float& x, float& y)
{
    const uint32_t i = (uint32_t)(pair % (phases ? phases : 1u)) + 1u;
    x = Halton(i, 2) - 0.5f; y = Halton(i, 3) - 0.5f;
}
// DLSS guidance: phase count = 8 * (output / render)^2 per axis ratio, at least 8
inline uint32_t Phases(uint32_t u)
{
    const double r = Ratio(u);
    const uint32_t n = (uint32_t)floor(8.0 / (r * r) + 0.5);
    return n < 8u ? 8u : n;
}

 
inline void Mul(const float a[16], const float b[16], float out[16])
{
    float t[16];
    for(int c = 0; c < 4; ++c)
        for(int r = 0; r < 4; ++r)
            t[c * 4 + r] = a[r] * b[c * 4] + a[4 + r] * b[c * 4 + 1] + a[8 + r] * b[c * 4 + 2] + a[12 + r] * b[c * 4 + 3];
    for(int i = 0; i < 16; ++i) out[i] = t[i];
}
inline void Transform(const float m[16], const float v[4], float out[4])
{
    for(int r = 0; r < 4; ++r) out[r] = m[r] * v[0] + m[4 + r] * v[1] + m[8 + r] * v[2] + m[12 + r] * v[3];
}
// Gauss-Jordan in double; false when singular
inline bool Invert(const float m[16], float out[16])
{
    double a[4][8];
    for(int r = 0; r < 4; ++r)
        for(int c = 0; c < 4; ++c) { a[r][c] = m[c * 4 + r]; a[r][4 + c] = r == c ? 1.0 : 0.0; }
    for(int c = 0; c < 4; ++c)
    {
        int p = c;
        for(int r = c + 1; r < 4; ++r) if(fabs(a[r][c]) > fabs(a[p][c])) p = r;
        if(fabs(a[p][c]) < 1e-30) return false;
        if(p != c) for(int k = 0; k < 8; ++k) { const double t = a[c][k]; a[c][k] = a[p][k]; a[p][k] = t; }
        const double d = a[c][c];
        for(int k = 0; k < 8; ++k) a[c][k] /= d;
        for(int r = 0; r < 4; ++r)
            if(r != c) { const double f = a[r][c]; if(f != 0.0) for(int k = 0; k < 8; ++k) a[r][k] -= f * a[c][k]; }
    }
    for(int r = 0; r < 4; ++r)
        for(int c = 0; c < 4; ++c) out[c * 4 + r] = (float)a[r][4 + c];
    return true;
}
// The image shift (render pixels, +x right, +y down) that the projection `jittered` applies relative to `plain`, measured in VIEW
// space at a point one metre in front of the camera (the z sign whose w is positive, so either camera convention works). A
// projection shear moves every depth by the same amount, and view space keeps full float precision (world coordinates of
// thousands of metres would not; first control run, 2026-11-03).
inline bool MeasureJitter(const float plain[16], const float jittered[16], uint32_t w, uint32_t h, float& x, float& y)
{
    x = y = 0.f;
    float v[4] = {0.f, 0.f, -1.f, 1.f}, a[4], b[4];
    Transform(plain, v, a);
    if(a[3] <= 0.f) { v[2] = 1.f; Transform(plain, v, a); }
    Transform(jittered, v, b);
    if(a[3] <= 1e-12f || b[3] <= 1e-12f) return false;
    x = (b[0] / b[3] - a[0] / a[3]) * 0.5f * (float)w;
    y = -(b[1] / b[3] - a[1] / a[3]) * 0.5f * (float)h;
    return true;
}
// Camera position of a rigid view matrix [R|t] in double: -R^T t (column-major: R(r,c) = view[4c+r], t = column 3)
inline void CameraOrigin(const float view[16], double o[3])
{
    for(int c = 0; c < 3; ++c)
        o[c] = -((double)view[c * 4] * view[12] + (double)view[c * 4 + 1] * view[13] + (double)view[c * 4 + 2] * view[14]);
}
 
inline void CameraForward(const float view[16], float f[3])
{
    for(int c = 0; c < 3; ++c) f[c] = -view[c * 4 + 2];
}
// proj * view * Translate(origin) computed in double: the composite of a point given relative to `origin`. Only the final,
// small-translation result is rounded to float (world composites of thousands of metres lose the sub-pixel motion).
inline void RelativeComposite(const float proj[16], const float view[16], const double origin[3], float out[16])
{
    double v[16], t[16];
    for(int i = 0; i < 16; ++i) v[i] = view[i];
    for(int r = 0; r < 4; ++r) v[12 + r] = view[r] * origin[0] + view[4 + r] * origin[1] + view[8 + r] * origin[2] + (double)view[12 + r];
    for(int c = 0; c < 4; ++c)
        for(int r = 0; r < 4; ++r)
            t[c * 4 + r] = proj[r] * v[c * 4] + proj[4 + r] * v[c * 4 + 1] + proj[8 + r] * v[c * 4 + 2] + proj[12 + r] * v[c * 4 + 3];
    for(int i = 0; i < 16; ++i) out[i] = (float)t[i];
}
// History reset between two consecutive guides of one eye: not the next pair, or the camera jumped (cut / teleport) by more
// than `metres` or turned more than ~60 degrees (forward dot < 0.5). Camera position/forward come from the inverse view.
inline bool NeedsReset(bool hadPrevious, uint64_t prevGeneration, uint64_t generation, const float prevPos[3], const float pos[3],
                       const float prevFwd[3], const float fwd[3], float metres)
{
    if(!hadPrevious || generation != prevGeneration + 1) return true;
    const float dx = pos[0] - prevPos[0], dy = pos[1] - prevPos[1], dz = pos[2] - prevPos[2];
    if(dx * dx + dy * dy + dz * dz > metres * metres) return true;
    return prevFwd[0] * fwd[0] + prevFwd[1] * fwd[1] + prevFwd[2] * fwd[2] < 0.5f;
}

}
