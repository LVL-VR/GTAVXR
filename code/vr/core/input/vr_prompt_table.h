#pragma once
 
 
//   pad level  : the base Quest -> pad layout (xr/locomotion.h Mapping: A/B/X/Y, stick clicks, grips = LB/RB, triggers =
//                LT/RT, short Menu = Start, Menu + X = Back, both stick clicks + right stick = D-pad), after the user's 165
 
//                (input/phone_controls.h).
 
//                context L3+A, secondary L3+B, special ability double L3, cover L3 held; input/phone_controls.h R3 phone;
 
#include "vr_prompts.h"
#include "button_map.h"
namespace gx_vr_prompts
{
// short labels (<= 12 bytes: TextFormat's icon text buffer is 15 with "t_" and the terminator), UTF-8
inline const char* label(unsigned g, bool es)
{
    static const char* const en[GLYPH_COUNT - TEXT_FIRST] = {"L GRIP",    "R GRIP", "L TRIGGER",
                                                             "R TRIGGER", "MENU",   "HOLSTER"};
    static const char* const sp[GLYPH_COUNT - TEXT_FIRST] = {"AGARRE IZQ",  "AGARRE DER",  "GATILLO IZQ",
                                                             "GATILLO DER", "MEN\303\232", "FUNDA"};
    return text(g) ? (es ? sp[g - TEXT_FIRST] : en[g - TEXT_FIRST]) : "";
}
// the Quest face button / stick click glyph of a 165 remap source
inline unsigned sourceGlyph(unsigned s)
{
    static const unsigned g[gx_button_map::SOURCE_COUNT] = {G_A, G_B, G_X, G_Y, G_L3, G_R3};
    return s < gx_button_map::SOURCE_COUNT ? g[s] : (unsigned)END;
}
// the first Quest source whose remapped target is `target`; a SAME source counts for its own slot `own` (-1: none)
inline unsigned sourceFor(const int* map, int target, int own)
{
    for (unsigned s = 0; s < gx_button_map::SOURCE_COUNT; ++s)
    {
        const int t = gx_button_map::sanitize(map[s]);
        if (t == target || (t == gx_button_map::SAME && own == (int)s))
            return sourceGlyph(s);
    }
    return END;
}
 
inline void padTable(const int* map, bool phone, Packed* out)
{
    const int faces[6][2] = {{S_A, gx_button_map::T_A},   {S_B, gx_button_map::T_B},
                             {S_X, gx_button_map::T_X},   {S_Y, gx_button_map::T_Y},
                             {S_L3, gx_button_map::T_L3}, {S_R3, gx_button_map::T_R3}};
    for (unsigned i = 0; i < SLOT_COUNT; ++i)
        out[i] = 0;
    for (unsigned f = 0; f < 6; ++f)
    {
        const unsigned g = sourceFor(map, faces[f][1], (int)f);
        out[faces[f][0]] = g ? pack(g) : 0;
    }
    out[S_LB] = pack(T_LGRIP);
    out[S_RB] = pack(T_RGRIP);
    out[S_LT] = pack(T_LTRIGGER);
    out[S_RT] = pack(T_RTRIGGER);
    out[S_START] = pack(T_MENU); // short press
    {
        const unsigned g = sourceFor(map, gx_button_map::T_BACK, -1);
        out[S_BACK] = g ? pack(g) : pack(T_MENU, G_X);
    } // Menu + X
    const int dirs[4][3] = {{S_UP, gx_button_map::T_UP, G_RSTICK_UP},
                            {S_DOWN, gx_button_map::T_DOWN, G_RSTICK_DOWN},
                            {S_LEFT, gx_button_map::T_LEFT, G_RSTICK_LEFT},
                            {S_RIGHT, gx_button_map::T_RIGHT, G_RSTICK_RIGHT}};
    for (unsigned d = 0; d < 4; ++d)
    {
        const unsigned g = sourceFor(map, dirs[d][1], -1); // both stick clicks + right stick
        out[dirs[d][0]] = phone ? pack(dirs[d][2]) : (g ? pack(g) : pack(G_L3, G_R3, dirs[d][2]));
    }
    out[S_DPAD_UPDOWN] = phone ? pack(G_RSTICK_UPDOWN) : pack(G_L3, G_R3, G_RSTICK_UPDOWN);
    out[S_DPAD_LEFTRIGHT] = phone ? pack(G_RSTICK_LEFTRIGHT) : pack(G_L3, G_R3, G_RSTICK_LEFTRIGHT);
    out[S_DPAD_ALL] = phone ? pack(G_RSTICK_ALL) : pack(G_L3, G_R3, G_RSTICK_ALL);
    if (phone)
    {
        out[S_A] = pack(G_A);
        out[S_B] = pack(G_B);
    } // phone_controls.h reads the Quest A / B themselves
}
// seated: vehicle / aircraft (the left-hand holster is the radio wheel there). motionOff (item 5 MOTION CONTROLS OFF): no
 
inline Packed inputRow(unsigned input, bool seated, bool motionOff)
{
    if (motionOff && (input == I_SELECT_WEAPON || input == I_RADIO_WHEEL))
        return 0;
    switch (input)
    {
    case I_CONTEXT:
        return pack(G_L3, G_A);
    case I_CONTEXT_SECONDARY:
        return pack(G_L3, G_B);
    case I_SPECIAL_ABILITY:
        return pack(G_L3, G_L3); // double L3 (300 ms)
    case I_COVER:
        return seated ? 0 : pack(G_L3); // L3 held, on foot
    case I_PHONE:
        return pack(G_R3);
    case I_SELECT_WEAPON:
        return pack(T_HOLSTER);
    case I_RADIO_WHEEL:
        return seated ? pack(T_HOLSTER) : 0;
    default:
        return 0;
    }
}
} // namespace gx_vr_prompts
