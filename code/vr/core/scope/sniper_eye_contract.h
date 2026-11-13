#pragma once
// Build141 E CONTRACT (frozen by Claude before delegation; implementation lives in sniper_eye.h).
// Monocular sniper presentation POLICY only: which eye shows the telescopic image. Pure, VS2012-compatible,
// no game/runtime/XR state. User brief: Right / Left / Auto (Nearest); Auto picks the eye nearest the ocular
// when support starts and KEEPS it for that hold; recomputed only after release + new support. No eye tracking.
#include "../physics/physics_contact.h"
namespace gx_sniper_eye
{
using gx_physics::V;
enum Setting
{
    SCOPE_EYE_RIGHT = 0,
    SCOPE_EYE_LEFT = 1,
    SCOPE_EYE_AUTO = 2,
    SCOPE_EYE_SETTING_COUNT = 3
};
// OpenXR primary stereo view order: index 0 = LEFT eye, 1 = RIGHT eye.
enum Eye
{
    EYE_LEFT = 0,
    EYE_RIGHT = 1,
    EYE_NONE = 2
};
// Persisted value -> Setting. Unknown/out-of-range values -> fallback (caller passes the implementation default; the
// user has NOT chosen a default yet, so it must never be presented as an accepted choice).
inline unsigned parseSetting(int stored, unsigned fallback);
// Short EN/ES labels and one-line help for the VR menu (static strings, never NULL for a valid Setting).
inline const char* settingLabel(unsigned setting, bool spanish);
inline const char* settingHelp(unsigned setting, bool spanish);

struct Inputs
{
    bool support;     // SUCCESSFUL secondary-hand support from the existing solver (never a raw trigger)
    bool sniper;      // held weapon is a qualified sniper (GROUP_SNIPER) with an active native scope
    bool valid;       // fresh, coherent poses this frame (else the mode must drop)
    unsigned setting; // Setting
    V eye[2];         // current eye positions, SAME space as ocular (OpenXR view order)
    V ocular;         // ocular / valid scope reference point in that space
    Inputs() : support(false), sniper(false), valid(false), setting(SCOPE_EYE_AUTO) {}
};
class EyeLock
{
  public:
    EyeLock();
    void reset(); // pause, death, cinematic, XR restart, weapon/component change
    // Returns EYE_LEFT/EYE_RIGHT while the monocular mode is active, EYE_NONE otherwise.
    // Activation edge (support && sniper && valid becoming true): RIGHT/LEFT -> that eye; AUTO -> the eye with the
    // smaller distance to ocular (ties -> RIGHT). While active the eye is HELD (no switching by small motion), except
    // that changing the setting to RIGHT/LEFT mid-hold applies immediately (explicit override). Any of support/sniper/
    // valid false -> EYE_NONE immediately (release restores both eyes); a new activation re-evaluates.
    unsigned update(const Inputs& in);
    unsigned locked() const;
    bool active() const;

  private:
    bool active_;
    unsigned eye_;
    unsigned setting_;
};
}
