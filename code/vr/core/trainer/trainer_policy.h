#pragma once
// Scoped cheat override: restore only our own last write, not a newer script change.
namespace gx_trainer
{
// A registered/loaded vehicle model is not proof of its native render contract.
// Runtime12 faulted in SetAllLights through a NULL per-vehicle shader effect.
inline const char* modelShaderFailure(bool drawable, bool master, bool vehicleType)
{
    if (!drawable)
        return "MODEL_DRAWABLE_MISSING";
    if (!master)
        return "MODEL_SHADER_EFFECT_MISSING";
    if (!vehicleType)
        return "MODEL_SHADER_EFFECT_WRONG_TYPE";
    return 0;
}
inline const char* instanceShaderFailure(bool handler, bool effect, bool vehicleType)
{
    if (!handler)
        return "INSTANCE_DRAW_HANDLER_MISSING";
    if (!effect)
        return "INSTANCE_SHADER_EFFECT_MISSING";
    if (!vehicleType)
        return "INSTANCE_SHADER_EFFECT_WRONG_TYPE";
    return 0;
}
template <class T> class Override
{
    bool owned_;
    T before_, last_;

  public:
    Override() : owned_(false), before_(T()), last_(T()) {}
    bool owned() const
    {
        return owned_;
    }
    bool update(T current, bool enabled, T desired, T& next)
    {
        if (enabled)
        {
            if (!owned_)
            {
                before_ = current;
                owned_ = true;
            }
            last_ = desired;
            next = desired;
            return current != desired;
        }
        if (owned_)
        {
            owned_ = false;
            if (current == last_)
            {
                next = before_;
                return current != before_;
            }
        }
        next = current;
        return false;
    }
};
}
