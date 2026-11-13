#pragma once
#include <stdint.h>
namespace menyoo_vr
{
enum LanguageId
{
    English = 0,
    Spanish = 1
};
enum ColourRole
{
    Header,
    Background,
    TitleText,
    OptionText,
    SelectedText,
    Selection,
    HelpText,
    ColourRoleCount
};
struct Colour
{
    uint32_t r, g, b, a;
};
struct Appearance
{
    uint32_t size, version, language, reserved;
    Colour colour[ColourRoleCount];
};
// Supplied by the integration host; excluded data/code stay outside the core.
Appearance defaultAppearance();
inline bool validAppearance(const Appearance& a)
{
    if (a.size != sizeof(a) || a.version != 1 || a.language > Spanish || a.reserved)
        return false;
    for (unsigned i = 0; i < ColourRoleCount; ++i)
    {
        const Colour& c = a.colour[i];
        if (c.r > 255 || c.g > 255 || c.b > 255 || c.a > 255)
            return false;
    }
    return true;
}
}
