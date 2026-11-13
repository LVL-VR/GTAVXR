#pragma once
#include <windows.h>
#include <stdio.h>
#include "../../../core/ui/ui_element_calibration.h"
#include "../../../core/ui/ui_visibility.h"
#include <stdint.h>
#include <string.h>

namespace gx_ui_element
{
inline int readValue(const char* path, const char* key, int fallback, int low, int high, const char* section);
inline GxUiVisibility loadVisibility(const char* path)
{
    GxUiVisibility p = gx_ui_visibility::defaults();
    if (!path)
        return p;
    const int mask =
        readValue(path, "mask", gx_ui_visibility::All, 0, gx_ui_visibility::All, "UiVisibility1");
    if (mask >= 0 && !(unsigned(mask) & ~unsigned(gx_ui_visibility::All)))
        p.mask = unsigned(mask);
    const int size = readValue(path, "percent", 100, 40, 200, "PausePanel1");
    if (size >= 40 && size <= 200)
        p.pausePercent = size;
    return p;
}
inline bool saveVisibility(const char* path, const GxUiVisibility& p)
{
    if (!path || !gx_ui_visibility::valid(p))
        return false;
    char value[32];
    sprintf_s(value, "%u", p.mask);
    if (!WritePrivateProfileStringA("UiVisibility1", "mask", value, path))
        return false;
    sprintf_s(value, "%d", p.pausePercent);
    return WritePrivateProfileStringA("PausePanel1", "percent", value, path) != 0;
}
static const char* const ElementNames[Count] = {"money",    "notifications", "phone", "help",  "subtitles",
                                                "location", "status",        "other", "wanted"};

inline bool strictInteger(const char* text, int low, int high, int& value)
{
    if (!text || !*text)
        return false;
    const char* p = text;
    bool negative = false;
    if (*p == '-' || *p == '+')
    {
        negative = *p == '-';
        ++p;
    }
    if (!*p)
        return false;
    unsigned long long magnitude = 0;
    for (; *p; ++p)
    {
        if (*p < '0' || *p > '9')
            return false;
        const unsigned digit = (unsigned)(*p - '0');
        if (magnitude > (2147483648ULL - digit) / 10ULL)
            return false;
        magnitude = magnitude * 10ULL + digit;
    }
    const long long parsed = negative ? -(long long)magnitude : (long long)magnitude;
    if (parsed < low || parsed > high)
        return false;
    value = (int)parsed;
    return true;
}

inline bool readInteger(const char* path, const char* key, int fallback, int low, int high, int& value,
                        const char* section = "WristElements1")
{
    value = fallback;
    if (!path || !key)
        return false;
    char text[48] = {};
    const DWORD length = GetPrivateProfileStringA(section, key, "", text, (DWORD)sizeof(text), path);
    if (length == 0)
        return true; // missing or empty: retain this key's default only
    if (length >= sizeof(text) - 1)
        return false; // a truncated/oversized value is malformed
    return strictInteger(text, low, high, value);
}

inline int readValue(const char* path, const char* key, int fallback, int low, int high,
                     const char* section = "WristElements1")
{
    int value = fallback;
    readInteger(path, key, fallback, low, high, value, section);
    return value;
}

inline void makeKey(char (&key)[64], const char* element, const char* field)
{
    sprintf_s(key, sizeof(key), "%s_%s", element, field);
}

inline GxUiElementProfiles loadElements(const char* path)
{
    GxUiElementProfiles profiles = defaults();
    if (!path)
        return profiles;
    if (readValue(path, "version", 0, 1, 1, "NativeMenu1") == 1)
        profiles.menuPercent = readValue(path, "percent", 100, 40, 200, "NativeMenu1");
    static const char* const axis[3] = {"x", "y", "z"};
    for (unsigned kind = 0; kind < Count; ++kind)
    {
        const char* section = kind == Wanted ? "WristWanted1" : "WristElements1";
        if (readValue(path, "version", 0, 1, 1, section) != 1)
            continue;
        GxUiElementProfile& profile = profiles.element[kind];
        char key[64];
        makeKey(key, ElementNames[kind], "individual");
        profile.individual = (uint32_t)readValue(path, key, 0, 0, 1, section);
        makeKey(key, ElementNames[kind], "percent");
        profile.percent = readValue(path, key, 100, 40, 200, section);
        for (unsigned coordinate = 0; coordinate < 3; ++coordinate)
        {
            char field[24];
            sprintf_s(field, sizeof(field), "mm_%s", axis[coordinate]);
            makeKey(key, ElementNames[kind], field);
            profile.mm[coordinate] = readValue(path, key, 0, -300, 300, section);
            sprintf_s(field, sizeof(field), "degrees_%s", axis[coordinate]);
            makeKey(key, ElementNames[kind], field);
            profile.degrees[coordinate] = readValue(path, key, 0, -180, 180, section);
        }
    }
    return valid(profiles) ? profiles : defaults();
}

inline bool appendPair(char* section, size_t capacity, size_t& used, const char* key, const char* value)
{
    if (!section || !key || !value)
        return false;
    const size_t keyLength = strlen(key), valueLength = strlen(value);
    if (used + keyLength + valueLength + 2 >= capacity)
        return false;
    memcpy(section + used, key, keyLength);
    used += keyLength;
    section[used++] = '=';
    memcpy(section + used, value, valueLength);
    used += valueLength;
    section[used++] = 0;
    return true;
}

inline bool appendInteger(char* section, size_t capacity, size_t& used, const char* key, int value)
{
    char text[24];
    if (sprintf_s(text, sizeof(text), "%d", value) <= 0)
        return false;
    return appendPair(section, capacity, used, key, text);
}

inline bool saveElements(const char* path, const GxUiElementProfiles& profiles)
{
    if (!path || !valid(profiles))
        return false;
    char menuValue[24];
    sprintf_s(menuValue, "%d", profiles.menuPercent);
    if (!WritePrivateProfileStringA("NativeMenu1", "percent", menuValue, path) ||
        !WritePrivateProfileStringA("NativeMenu1", "version", "1", path))
        return false;
    char section[4096] = {};
    size_t used = 0;
    if (!appendInteger(section, sizeof(section), used, "version", 1))
        return false;
    static const char* const axis[3] = {"x", "y", "z"};
    for (unsigned kind = 0; kind < Count; ++kind)
    {
        if (kind == Wanted)
        {
            section[used] = 0;
            if (!WritePrivateProfileSectionA("WristElements1", section, path))
                return false;
            memset(section, 0, sizeof(section));
            used = 0;
            if (!appendInteger(section, sizeof(section), used, "version", 1))
                return false;
        }
        const GxUiElementProfile& profile = profiles.element[kind];
        char key[64], field[24];
        makeKey(key, ElementNames[kind], "individual");
        if (!appendInteger(section, sizeof(section), used, key, (int)profile.individual))
            return false;
        makeKey(key, ElementNames[kind], "percent");
        if (!appendInteger(section, sizeof(section), used, key, profile.percent))
            return false;
        for (unsigned coordinate = 0; coordinate < 3; ++coordinate)
        {
            sprintf_s(field, sizeof(field), "mm_%s", axis[coordinate]);
            makeKey(key, ElementNames[kind], field);
            if (!appendInteger(section, sizeof(section), used, key, profile.mm[coordinate]))
                return false;
            sprintf_s(field, sizeof(field), "degrees_%s", axis[coordinate]);
            makeKey(key, ElementNames[kind], field);
            if (!appendInteger(section, sizeof(section), used, key, profile.degrees[coordinate]))
                return false;
        }
    }
    if (used + 1 >= sizeof(section))
        return false;
    section[used] = 0; // WritePrivateProfileSectionA requires a double-NUL list.
    // Separate native section survives an older candidate writing its eight
    // RIGHT profiles.83 never overwrites unrelated calibrations/settings.
    return WritePrivateProfileSectionA("WristWanted1", section, path) != 0;
}
}
