#pragma once
#include "weapon_calibration.h"
#include "../hands/model_calibration_win.h"
namespace gx_weapon_calibration
{
class IniBackend : public Backend
{
    const char* path_;
    gx_profile::Backend& legacy_;
    unsigned reads_, writes_;
    // A reference-bound backend cannot be reassigned. Explicit for v110 /W4.
    IniBackend& operator=(const IniBackend&);
    static bool section(const Key& k, char* out, size_t n)
    {
        return k.valid() && sprintf_s(out, n, "WeaponAnchor.%s.%s.%s.%08X", k.hand ? "Right" : "Left",
                                      k.anchor == WEAPON ? "Weapon" : "Support",
                                      k.family ? "Family" : "Exact", k.hash) > 0;
    }

  public:
    IniBackend(const char* path, gx_profile::Backend& legacy)
        : path_(path), legacy_(legacy), reads_(0), writes_(0)
    {
    }
    Record load(const Key& k)
    {
        Record r;
        char name[96], text[192];
        ++reads_;
        if (!path_ || !section(k, name, sizeof(name)))
        {
            r.loaded = INVALID;
            r.saved = false;
            return r;
        }
        const DWORD count =
            GetPrivateProfileStringA(name, "parameters", "<absent>", text, sizeof(text), path_);
        if (strcmp(text, "<absent>"))
        {
            if (count >= sizeof(text) - 1 || !decode(text, k, r))
            {
                r = Record();
                r.loaded = INVALID;
                r.saved = false;
            }
        }
        // READ ONLY old K5/K6. Never reinterpret Kind2 transforms or rewrite accepted points.
        if (k.hand == 1 && k.anchor == SUPPORT)
        {
            gx_profile::Pose old;
            const gx_profile::Loaded loaded = legacy_.load(
                gx_profile::Key(
                    k.family ? gx_profile::WEAPON_SUPPORT_CLASS : gx_profile::WEAPON_SUPPORT_POINT, k.hash),
                old);
            if (loaded == gx_profile::PRESENT)
            {
                r.legacyMask = 7;
                for (unsigned p = 0; p < 3; ++p)
                    r.legacy.value[p] = old.mm[p];
            }
            else if (loaded == gx_profile::INVALID)
                r.saved = false;
        }
        return r;
    }
    bool write(const Key& k, const Record* r)
    {
        char name[96], text[160];
        if (!path_ || !section(k, name, sizeof(name)) || (r && !encode(k, *r, text, sizeof(text))))
            return false;
        ++writes_;
        if (!WritePrivateProfileStringA(name, "parameters", r ? text : NULL, path_))
            return false;
        WritePrivateProfileStringA(NULL, NULL, NULL, path_);
        const Record observed = load(k);
        return r ? observed.loaded == PRESENT && observed.sameStored(*r) : observed.loaded == ABSENT;
    }
    unsigned reads() const
    {
        return reads_;
    }
    unsigned writes() const
    {
        return writes_;
    }
};
}
