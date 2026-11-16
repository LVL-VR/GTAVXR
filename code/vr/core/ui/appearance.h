#pragma once
#include <stdint.h>
namespace menyoo_vr
{
enum LanguageId
{
    English = 0,
    Spanish = 1,
    Portuguese = 2 // Brazilian Portuguese
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
// Menu language option: English -> Spanish -> Portuguese -> English.
inline uint32_t nextLanguage(uint32_t language)
{
    return language == English ? Spanish : (language == Spanish ? Portuguese : English);
}
// Supplied by the integration host; excluded data/code stay outside the core.
Appearance defaultAppearance();
inline bool validAppearance(const Appearance& a)
{
    if (a.size != sizeof(a) || a.version != 1 || a.language > Portuguese || a.reserved)
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
