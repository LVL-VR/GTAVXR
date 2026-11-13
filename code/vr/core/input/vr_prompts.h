#pragma once
 
// CORRECTO ASIGNADO A CADA ACCION EN VEZ DE LOS DEFAULT DE XBOX (POR EJEMPLO QUE EL ACCIONAR MAQUINA EXPENDEDORA MUESTRE EL
// ICONO/ILUSTRACION L3+A ...)"; answers: short text for grip / trigger / menu / gestures in the VR language, ON by default).
 
// instructional buttons all end in GetTokenIDFromPadButton / GetInputButtons) and the VR binding table (vr_prompt_table.h,
 
namespace gx_vr_prompts
{
enum Glyph
{
    END = 0,
    G_A,
    G_B,
    G_X,
    G_Y,
    G_L3,
    G_R3,
    G_RSTICK_UP,
    G_RSTICK_DOWN,
    G_RSTICK_LEFT,
    G_RSTICK_RIGHT,
    G_RSTICK_UPDOWN,
    G_RSTICK_LEFTRIGHT,
    G_RSTICK_ALL,
    TEXT_FIRST,
    T_LGRIP = TEXT_FIRST,
    T_RGRIP,
    T_LTRIGGER,
    T_RTRIGGER,
    T_MENU,
    T_HOLSTER,
    GLYPH_COUNT
};
 
enum Slot
{
    S_A,
    S_B,
    S_X,
    S_Y,
    S_LB,
    S_RB,
    S_LT,
    S_RT,
    S_L3,
    S_R3,
    S_START,
    S_BACK,
    S_UP,
    S_DOWN,
    S_LEFT,
    S_RIGHT,
    S_DPAD_UPDOWN,
    S_DPAD_LEFTRIGHT,
    S_DPAD_ALL,
    SLOT_COUNT
};
 
enum Input
{
    I_CONTEXT,
    I_CONTEXT_SECONDARY,
    I_SPECIAL_ABILITY,
    I_COVER,
    I_PHONE,
    I_SELECT_WEAPON,
    I_RADIO_WHEEL,
    INPUT_COUNT
};
 
enum
{
    MAX_GLYPHS = 4,
    TEXT_ICON_BASE = 0x7F000000u
};
typedef unsigned Packed; // up to 4 glyphs, 8 bits each, first glyph in the low byte; 0 = no row
inline Packed pack(unsigned a, unsigned b = END, unsigned c = END, unsigned d = END)
{
    return (a & 255) | (b & 255) << 8 | (c & 255) << 16 | (d & 255) << 24;
}
inline unsigned glyph(Packed p, unsigned i)
{
    return i < MAX_GLYPHS ? (p >> (8 * i)) & 255 : END;
}
inline unsigned count(Packed p)
{
    unsigned n = 0;
    while (n < MAX_GLYPHS && glyph(p, n) != END)
        ++n;
    return n;
}
inline bool text(unsigned g)
{
    return g >= TEXT_FIRST && g < GLYPH_COUNT;
}
inline bool valid(Packed p)
{
    const unsigned n = count(p);
    if (!n)
        return false;
    for (unsigned i = 0; i < n; ++i)
        if (glyph(p, i) >= GLYPH_COUNT)
            return false;
    for (unsigned i = n; i < MAX_GLYPHS; ++i)
        if (glyph(p, i) != END)
            return false;
    return true;
}
} // namespace gx_vr_prompts
namespace GTAVXRNativePair
{
 
 
bool VrPromptActive();
unsigned VrPromptSlot(unsigned slot);
unsigned VrPromptInput(unsigned input);
const char* VrPromptText(unsigned glyph);
// a resolved prompt (kind 0 = pad slot, 1 = input), for the VRPROMPT167 / VRPROMPT167_MISSING counters (bounded)
void VrPromptSeen(unsigned kind, unsigned id, bool substituted);
}
