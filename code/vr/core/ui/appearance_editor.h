#pragma once
#include "appearance.h"
#include <string.h>
namespace menyoo_vr
{
inline bool sameAppearance(const Appearance& a, const Appearance& b)
{
    return memcmp(&a, &b, sizeof(a)) == 0;
}
inline void resetAppearance(Appearance& a)
{
    const uint32_t language = a.language;
    a = defaultAppearance();
    a.language = language <= Spanish ? language : English;
}
inline bool editColour(Appearance& a, unsigned role, unsigned channel, int delta)
{
    if (!validAppearance(a) || role >= ColourRoleCount || channel > 3)
        return false;
    uint32_t* channels[] = {&a.colour[role].r, &a.colour[role].g, &a.colour[role].b, &a.colour[role].a};
    int value = static_cast<int>(*channels[channel]) + delta;
    if (value < 0)
        value = 0;
    if (value > 255)
        value = 255;
    *channels[channel] = static_cast<uint32_t>(value);
    return true;
}
inline const char* colourLabel(unsigned role, uint32_t language)
{
    const char* en[] = {"Header",        "Background", "Title Text", "Option Text",
                        "Selected Text", "Selection",  "Help Text"};
    const char* es[] = {"Cabecera",
                        "Fondo",
                        "Texto del t\303\255tulo",
                        "Texto de opciones",
                        "Texto seleccionado",
                        "Selecci\303\263n",
                        "Texto de ayuda"};
    return role < ColourRoleCount ? (language == Spanish ? es[role] : en[role]) : "";
}
}
