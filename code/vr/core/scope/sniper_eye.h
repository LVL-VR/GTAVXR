#pragma once
 
// Claude. Review correction: help strings shortened to fit GxOverlay rows (char[80]); logic unchanged.
#include "sniper_eye_contract.h"

namespace gx_sniper_eye
{
inline unsigned parseSetting(int stored, unsigned fallback)
{
    return stored >= 0 && stored < SCOPE_EYE_SETTING_COUNT ? static_cast<unsigned>(stored) : fallback;
}

// UTF-8 escapes keep these static strings identical under both compiler code pages.
inline const char* settingLabel(unsigned setting, bool spanish)
{
    switch (setting)
    {
    case SCOPE_EYE_RIGHT:
        return spanish ? "Ojo de la mira: Derecho" : "Scope Eye: Right";
    case SCOPE_EYE_LEFT:
        return spanish ? "Ojo de la mira: Izquierdo" : "Scope Eye: Left";
    case SCOPE_EYE_AUTO:
        return spanish ? "Ojo de la mira: Autom\xC3\xA1tico (m\xC3\xA1s cercano)"
                       : "Scope Eye: Auto (Nearest)";
    default:
        return spanish ? "Ojo de la mira: Desconocido" : "Scope Eye: Unknown";
    }
}

inline const char* settingHelp(unsigned setting, bool spanish)
{
    switch (setting)
    {
    case SCOPE_EYE_RIGHT:
        return spanish ? "Mira en el ojo derecho mientras dure el apoyo."
                       : "Scope view in the right eye while supported.";
    case SCOPE_EYE_LEFT:
        return spanish ? "Mira en el ojo izquierdo mientras dure el apoyo."
                       : "Scope view in the left eye while supported.";
    case SCOPE_EYE_AUTO:
        return spanish ? "Al apoyar elige el ojo m\xC3\xA1s cercano; lo mantiene hasta soltar."
                       : "Nearest eye when support starts; kept until release.";
    default:
        return spanish ? "Derecho, Izquierdo o Autom\xC3\xA1tico (m\xC3\xA1s cercano)."
                       : "Right, Left or Auto (Nearest).";
    }
}

inline EyeLock::EyeLock() : active_(false), eye_(EYE_NONE), setting_(SCOPE_EYE_AUTO) {}
inline void EyeLock::reset()
{
    active_ = false;
    eye_ = EYE_NONE;
    setting_ = SCOPE_EYE_AUTO;
}
inline unsigned EyeLock::update(const Inputs& in)
{
    if (!in.support || !in.sniper || !in.valid)
    {
        reset();
        return eye_;
    }
    if (!active_)
    {
        if (in.setting == SCOPE_EYE_RIGHT)
            eye_ = EYE_RIGHT;
        else if (in.setting == SCOPE_EYE_LEFT)
            eye_ = EYE_LEFT;
        else
        {
            const V left = gx_physics::sub(in.eye[EYE_LEFT], in.ocular);
            const V right = gx_physics::sub(in.eye[EYE_RIGHT], in.ocular);
            eye_ = gx_physics::dot(left, left) < gx_physics::dot(right, right) ? EYE_LEFT : EYE_RIGHT;
        }
        active_ = true;
    }
    else if (in.setting != setting_)
    {
        if (in.setting == SCOPE_EYE_RIGHT)
            eye_ = EYE_RIGHT;
        else if (in.setting == SCOPE_EYE_LEFT)
            eye_ = EYE_LEFT;
        // Switching to Auto preserves this hold's eye, even after a fixed override.
    }
    setting_ = in.setting;
    return eye_;
}
inline unsigned EyeLock::locked() const
{
    return eye_;
}
inline bool EyeLock::active() const
{
    return active_;
}
}
