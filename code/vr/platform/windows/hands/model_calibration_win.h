#pragma once
// Shared, testable Windows INI backend. One packed key per stable model/seat profile.
#include "model_calibration.h"
#include <windows.h>
namespace gx_profile
{
class IniBackend : public Backend
{
    const char* path_;
    unsigned reads_, writes_, invalid_;
    static bool section(Key key, char* out, size_t size)
    {
        if (!key.valid())
            return false;
        return sprintf_s(out, size,
                         key.kind == VEHICLE_CAMERA
                             ? "VehicleCamera.%08X.Seat%d"
                             : (key.kind == WEAPON_SECOND_HAND
                                    ? "WeaponSecondHand.%08X.%d"
                                    : (key.kind == WEAPON_SUPPORT_POINT
                                           ? "WeaponSupportPoint.%08X.%d"
                                           : (key.kind == WEAPON_SUPPORT_CLASS
                                                  ? "WeaponSupportFamily.%08X.%d"
                                                  : (key.kind == CAR_REACH ? "CarReach.%u.%d"
                                                                           : "VehicleClass.%u.%d")))),
                         key.model, key.seat) > 0;
    }

  public:
    explicit IniBackend(const char* path) : path_(path), reads_(0), writes_(0), invalid_(0) {}
    Loaded load(const Key& key, Pose& out)
    {
        char name[64], value[160];
        if (!path_ || !section(key, name, sizeof(name)))
            return INVALID;
        ++reads_;
        const DWORD count = GetPrivateProfileStringA(name, "pose", "<absent>", value, sizeof(value), path_);
        if (!strcmp(value, "<absent>"))
            return ABSENT;
        if (count >= sizeof(value) - 1 || !decode(value, key.kind, out))
        {
            ++invalid_;
            return INVALID;
        }
        return PRESENT;
    }
    bool write(const Key& key, const Pose* p)
    {
        char name[64], value[128];
        if (!path_ || !section(key, name, sizeof(name)) || (p && !encode(*p, key.kind, value, sizeof(value))))
            return false;
        ++writes_;
        if (!WritePrivateProfileStringA(name, "pose", p ? value : NULL, path_))
            return false;
        // Flush the Windows profile cache before declaring transparent persistence.
        // Documented flush return is zero too: it is NOT a write-failure test.
        WritePrivateProfileStringA(NULL, NULL, NULL, path_);
        Pose observed;
        const Loaded state = load(key, observed);
        return p ? state == PRESENT && observed.same(*p) : state == ABSENT;
    }
    unsigned reads() const
    {
        return reads_;
    }
    unsigned writes() const
    {
        return writes_;
    }
    unsigned invalid() const
    {
        return invalid_;
    }
};
}
