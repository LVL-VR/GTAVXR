#pragma once
#include <windows.h>
#include <stdio.h>
#include "../../../core/ui/wrist_calibration.h"
namespace gx_wrist
{
inline GxWristProfiles load(const char* path)
{
    GxWristProfiles p = defaults();
    if (!path || GetPrivateProfileIntA("WristHUD1", "version", 0, path) != 1)
        return p;
    for (unsigned h = 0; h < 2; ++h)
    {
        char key[48];
        for (unsigned c = 0; c < 3; ++c)
        {
            sprintf_s(key, "%s_mm_%c", h ? "right" : "left", 'x' + c);
            const int mm = (int)GetPrivateProfileIntA("WristHUD1", key, 0, path);
            p.hand[h].mm[c] = mm >= -300 && mm <= 300 ? mm : 0;
            sprintf_s(key, "%s_degrees_%c", h ? "right" : "left", 'x' + c);
            const int degrees = (int)GetPrivateProfileIntA("WristHUD1", key, 0, path);
            p.hand[h].degrees[c] = degrees >= -180 && degrees <= 180 ? degrees : 0;
        }
        sprintf_s(key, "%s_percent", h ? "right" : "left");
        const int size = (int)GetPrivateProfileIntA("WristHUD1", key, 100, path);
        p.hand[h].percent = size >= 40 && size <= 200 ? size : 100;
    }
    return p;
}
inline bool save(const char* path, const GxWristProfiles& p)
{
    if (!path || !valid(p))
        return false;
    char section[1024] = {};
    size_t used = 0;
    const char* side[] = {"left", "right"};
    for (unsigned h = 0; h < 2; ++h)
    {
        for (unsigned c = 0; c < 3; ++c)
        {
            const int n = sprintf_s(section + used, sizeof(section) - used, "%s_mm_%c=%d", side[h], 'x' + c,
                                    p.hand[h].mm[c]);
            if (n < 0)
                return false;
            used += n + 1;
            const int r = sprintf_s(section + used, sizeof(section) - used, "%s_degrees_%c=%d", side[h],
                                    'x' + c, p.hand[h].degrees[c]);
            if (r < 0)
                return false;
            used += r + 1;
        }
        const int s =
            sprintf_s(section + used, sizeof(section) - used, "%s_percent=%d", side[h], p.hand[h].percent);
        if (s < 0)
            return false;
        used += s + 1;
    }
    const int v = sprintf_s(section + used, sizeof(section) - used, "version=1");
    if (v < 0 || used + v + 2 > sizeof(section))
        return false;
    used += v + 1;
    section[used] = 0;
    // One section replacement, not fourteen partial independent key updates.
    // Every unrelated VR/hand/weapon/vehicle section is retained by the INI API.
    return WritePrivateProfileSectionA("WristHUD1", section, path) != 0;
}
}
