#pragma once
// Small Windows INI adapter shared by the native menu and actual headless controls.
// No gameplay identity/profile keys and no parsing of unbounded numeric values.
#include "../../../core/physics/physical_tuning.h"
#include <Windows.h>

namespace gx_physical_tuning_ini
{
inline bool space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
inline int percentText(const char* text, bool body = false)
{
    if (!text)
        return 100;
    unsigned i = 0;
    while (i < 15 && space(text[i]))
        ++i;
    unsigned digits = 0, value = 0;
    while (i < 15 && text[i] >= '0' && text[i] <= '9')
    {
        if (++digits > 3)
            return 100;
        value = value * 10 + unsigned(text[i] - '0');
        ++i;
    }
    while (i < 15 && space(text[i]))
        ++i;
    if (!digits || i >= 15 || text[i])
        return 100;
    return body ? gx_physical_tuning::validatedBodyPercent((int)value)
                : gx_physical_tuning::validatedPercent((int)value);
}
inline int readPercent(const char* path, const char* key, bool body = false)
{
    if (!path || !path[0] || !key)
        return 100;
    char text[16] = {0};
    const DWORD length = GetPrivateProfileStringA("Physics1", key, "", text, sizeof(text), path);
    return !length || length >= sizeof(text) - 1 ? 100 : percentText(text, body);
}
inline gx_physical_tuning::Settings load(const char* path)
{
    gx_physical_tuning::Settings out;
    out.grabPercent = readPercent(path, "grab_strength_percent");
    out.pushPercent = readPercent(path, "push_strength_percent");
    out.bodyPercent = readPercent(path, "body_effort_percent", true);
    return out;
}
inline bool writePercent(const char* path, const char* key, int value, bool body = false)
{
    if (!path || !path[0] || !key)
        return false;
    const unsigned percent = (unsigned)(body ? gx_physical_tuning::validatedBodyPercent(value)
                                             : gx_physical_tuning::validatedPercent(value));
    char text[4] = {0};
    unsigned n = 0;
    if (percent >= 100)
        text[n++] = char('0' + percent / 100);
    text[n++] = char('0' + (percent / 10) % 10);
    text[n] = char('0' + percent % 10);
    return WritePrivateProfileStringA("Physics1", key, text, path) != FALSE;
}
inline bool save(const gx_physical_tuning::Settings& s, const char* path)
{
    const gx_physical_tuning::Settings safe = gx_physical_tuning::checked(s);
    // Checked scalar writes; legacy files default the new body key to100%.
    return writePercent(path, "grab_strength_percent", safe.grabPercent) &&
           writePercent(path, "push_strength_percent", safe.pushPercent) &&
           writePercent(path, "body_effort_percent", safe.bodyPercent, true);
}
}
