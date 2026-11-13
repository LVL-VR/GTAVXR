#pragma once
#ifndef GTAVXR_QUALITY_H
#define GTAVXR_QUALITY_H
// GTAVXR build141 C (F3): OPTIONAL quality page (VR Menu > Settings > Quality). Pure policy, modern + v110.
 
 
//  - INSTANT rows (FXAA, tessellation): individual scalar consumers, committed between completed eye pairs.
 
 
 
// Every editable value defaults to UNCHANGED (-1): opening the page forces/normalizes/saves nothing.
namespace gx_quality
{
enum Row
{
    FXAA = 0,
    TESSELLATION,
    VR_SCALE,
    SHADOWS,
    SOFT_SHADOWS,
    REFLECTIONS,
    MSAA,
    AMBIENT_OCCLUSION,
    WATER,
    POST_FX,
    GRASS,
    PARTICLES,
    TEXTURES,
    BACK,
    ROW_COUNT
}; // build164: the per-eye fixes submenu is gone (fixes are permanent)
enum Mode
{
    INSTANT = 0,
    RESTART_GAME = 1,
    GTA_RESET_UNVERIFIED = 2,
    NO_MODE = 3
};
const int Unchanged = -1;
inline unsigned mode(unsigned row)
{
    if (row == FXAA || row == TESSELLATION)
        return INSTANT;
    if (row == VR_SCALE || row == GRASS || row == PARTICLES || row == TEXTURES)
        return RESTART_GAME;
    if (row >= SHADOWS && row <= POST_FX)
        return GTA_RESET_UNVERIFIED;
    return NO_MODE;
}
inline bool editable(unsigned row)
{
    return row == FXAA || row == TESSELLATION || row == VR_SCALE;
}
inline const char* modeLabel(unsigned m, bool es)
{
    if (m == INSTANT)
        return es ? "al instante" : "instant";
    if (m == RESTART_GAME)
        return es ? "reiniciar juego" : "restart game";
    if (m == GTA_RESET_UNVERIFIED)
        return es ? "solo lectura: reinicio de gráficos no verificado en VR"
                  : "read-only: graphics reset unverified in VR";
    return "";
}
inline const char* rowName(unsigned row, bool es)
{
    static const char* const en[ROW_COUNT] = {
        "FXAA",        "Tessellation", "VR render scale",   "Shadows", "Soft shadows",
        "Reflections", "MSAA",         "Ambient occlusion", "Water",   "Post FX",
        "Grass",       "Particles",    "Textures",          "<  Back"};
    static const char* const sp[ROW_COUNT] = {"FXAA",
                                              "Teselado",
                                              "Escala render VR",
                                              "Sombras",
                                              "Sombras suaves",
                                              "Reflejos",
                                              "MSAA",
                                              "Oclusi\303\263n ambiental",
                                              "Agua",
                                              "Post FX",
                                              "Hierba",
                                              "Part\303\255culas",
                                              "Texturas",
                                              "<  Volver"};
    return row < ROW_COUNT ? (es ? sp[row] : en[row]) : "?";
}
struct Overrides
{
    int fxaa, tessellation,
        vrScale; // Unchanged, or FXAA 0/1, tessellation 0..3, VR scale 50..150 step 10 (percent)
    Overrides() : fxaa(Unchanged), tessellation(Unchanged), vrScale(Unchanged) {}
    bool same(const Overrides& o) const
    {
        return fxaa == o.fxaa && tessellation == o.tessellation && vrScale == o.vrScale;
    }
};
// Menu cycles: Unchanged -> values -> Unchanged.
inline int nextFxaa(int v)
{
    return v < 0 ? 0 : (v == 0 ? 1 : Unchanged);
}
inline int nextTessellation(int v)
{
    return v < 0 ? 0 : (v >= 3 ? Unchanged : v + 1);
}
inline int nextVrScale(int v)
{
    return v < 0 ? 50 : (v >= 150 ? Unchanged : ((v / 10) + 1) * 10);
}
// Stored/INI value -> valid override (anything invalid = Unchanged; never clamps a bad value into a forced setting).
inline int parseFxaa(int v)
{
    return v == 0 || v == 1 ? v : Unchanged;
}
inline int parseTessellation(int v)
{
    return v >= 0 && v <= 3 ? v : Unchanged;
}
inline const char* tessellationName(int v, bool es)
{
    static const char* const en[4] = {"Off", "Normal", "High", "Very high"};
    static const char* const sp[4] = {"No", "Normal", "Alto", "Muy alto"};
    return v >= 0 && v <= 3 ? (es ? sp[v] : en[v]) : "?";
}
 
inline bool effectiveFxaa(int over, bool hostValue)
{
    return over < 0 ? hostValue : over == 1;
}
inline int effectiveTessellation(int over, int hostValue)
{
    return over < 0 ? hostValue : over;
}
}
#endif
