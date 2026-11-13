#pragma once
#include <string.h>
// Only known high-volume SUCCESS observations are suppressible. Unknown events
// and explicit faults remain visible; an optional trace flag never changes safety.
namespace gx_diagnostics
{
inline bool nativeEvent(const char* event, bool detailed, bool fault = false)
{
    if (!event)
        return false;
    if (detailed || fault)
        return true;
    const char* sampled[] = {"PRE_VISIBILITY_HMD_CENTER",
                             "PRE_VISIBILITY_SCOPE143",
                             "PHYSICS_NATIVE_QUERY_COUNT",
                             "PHYSICS_RIGHT_STATUS",
                             "PHYSICS_LEFT_STATUS",
                             "FORCE_GRAB_RIGHT_NATIVE_BOUNDED",
                             "FORCE_GRAB_LEFT_NATIVE_BOUNDED",
                             "WEAPON_COLLISION_RIGHT_STATUS",
                             "WEAPON_COLLISION_LEFT_STATUS",
                             "TRACKED_FLIGHT_CONTROLS",
                             "MOUNTED_TRACKED_TARGET_NATIVE_LIMITS",
                             "MOTORCYCLE_CONTROL_REFERENCE_NATIVE_FALLBACK"};
    for (unsigned n = 0; n < sizeof(sampled) / sizeof(sampled[0]); ++n)
        if (!strcmp(event, sampled[n]))
            return false;
    return true;
}
}

// Render-thread-owned diagnostic state only. Count the already sparse eligible
// success observations, per component and activation; no clock, heap or I/O.
// DetailedDiagnostics restores every eligible sample. Failures and unknown
// keys after capacity exhaustion remain visible regardless of this budget.
namespace gx_diagnostics
{
class SuccessSamples
{
    struct Slot
    {
        unsigned key, count;
    } slots_[64];
    unsigned serial_, used_;

  public:
    SuccessSamples() : serial_(0), used_(0) {}
    bool sample(unsigned serial, unsigned key, bool detailed, bool success, unsigned& ordinal)
    {
        if (serial_ != serial)
        {
            serial_ = serial;
            used_ = 0;
        }
        unsigned at = 0;
        for (; at < used_; ++at)
            if (slots_[at].key == key)
                break;
        ordinal = 0;
        if (at == used_)
        {
            if (used_ == sizeof(slots_) / sizeof(slots_[0]))
                return true;
            slots_[at].key = key;
            slots_[at].count = 0;
            ++used_;
        }
        if (!success)
        {
            ordinal = slots_[at].count;
            return true;
        }
        if (slots_[at].count == ~0u)
        {
            ordinal = slots_[at].count;
            return true;
        }
        ordinal = ++slots_[at].count;
        return detailed || ordinal <= 4 || ordinal % 32 == 0;
    }
};
}
