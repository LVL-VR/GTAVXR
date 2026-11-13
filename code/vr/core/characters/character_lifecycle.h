#pragma once
// Owned, v110-compatible policy. No native object, Story, save or pose writes.
#include <stdint.h>
namespace gx_character_lifecycle
{
enum Joint
{
    PELVIS,
    SPINE_ROOT,
    SPINE0,
    SPINE1,
    SPINE2,
    SPINE3,
    L_CLAVICLE,
    R_CLAVICLE,
    HEAD,
    L_EYE,
    R_EYE,
    L_UPPER,
    R_UPPER,
    L_FORE,
    R_FORE,
    L_HAND,
    R_HAND,
    L_PH,
    R_PH,
    L_IK,
    R_IK,
    L_THIGH,
    R_THIGH,
    L_CALF,
    R_CALF,
    L_FOOT,
    R_FOOT,
    JOINT_COUNT
};
enum EyeReference
{
    NO_REFERENCE,
    EYE_PAIR,
    HEAD_BIND
};
struct Layout
{
    int index[JOINT_COUNT], parent[JOINT_COUNT];
    unsigned end[JOINT_COUNT], bones;
    bool human;
    Layout() : bones(0), human(false)
    {
        for (unsigned i = 0; i < JOINT_COUNT; ++i)
        {
            index[i] = parent[i] = -1;
            end[i] = 0;
        }
    }
    bool valid(unsigned j) const
    {
        return j < JOINT_COUNT && index[j] >= 0 && (unsigned)index[j] < bones &&
               ((index[j] == 0 && parent[j] == -1) || (parent[j] >= 0 && parent[j] < index[j])) &&
               end[j] > (unsigned)index[j] && end[j] <= bones;
    }
    bool compatible() const
    {
        if (!human || !bones || bones > 4096)
            return false;
        for (unsigned j = 0; j < JOINT_COUNT; ++j)
        {
            if (j == L_EYE || j == R_EYE)
                continue;
            if (!valid(j))
                return false;
            for (unsigned k = 0; k < j; ++k)
                if (k != L_EYE && k != R_EYE && index[j] == index[k])
                    return false;
        }
        return true;
    }
    EyeReference eye() const
    {
        return !compatible()
                   ? NO_REFERENCE
                   : (valid(L_EYE) && valid(R_EYE) && index[L_EYE] != index[R_EYE] ? EYE_PAIR : HEAD_BIND);
    }
    bool headReferenceCompatible() const
    {
        // NeutralEye reads only HEAD and the two upper-arm bind positions.
        // Torso/gait/IK tags remain independently guarded by their consumers;
        // they must not disable all XR input for a human without facial eyes.
        if (!human || !bones || bones > 4096)
            return false;
        const unsigned required[] = {HEAD, L_UPPER, R_UPPER};
        for (unsigned j = 0; j < 3; ++j)
        {
            if (!valid(required[j]))
                return false;
            for (unsigned k = 0; k < j; ++k)
                if (index[required[j]] == index[required[k]])
                    return false;
        }
        return true;
    }
    unsigned signature() const
    {
        unsigned h = 2166136261u;
        for (unsigned i = 0; i < JOINT_COUNT; ++i)
        {
            h = (h ^ (unsigned)(index[i] + 1)) * 16777619u;
            h = (h ^ (unsigned)(parent[i] + 1)) * 16777619u;
            h = (h ^ end[i]) * 16777619u;
        }
        return h;
    }
};
template <class Parents> inline bool headReferenceReady(const Layout& layout, const Parents& parents)
{
    if (!layout.headReferenceCompatible() || parents.count() != layout.bones)
        return false;
    const unsigned required[] = {HEAD, L_UPPER, R_UPPER};
    for (unsigned j = 0; j < 3; ++j)
    {
        int bone = layout.index[required[j]];
        bool root = false;
        for (unsigned n = 0; n < layout.bones; ++n)
        {
            const int parent = parents.parent(bone);
            if (parent == -1)
            {
                root = bone == 0;
                break;
            }
            if (parent < 0 || parent >= bone || (unsigned)parent >= layout.bones)
                break;
            bone = parent;
        }
        if (!root)
            return false;
    }
    return true;
}
inline bool shaderRoute(bool streamed, bool variation, bool effectPresent, bool pedEffect)
{
    // Native streamed peds deliberately have no model-level effect. A foreign
    // non-null effect is NEVER treated as that native absence.
    if (streamed && !variation)
        return false;
    return effectPresent ? pedEffect : (streamed && variation);
}
inline const char* shaderGroupFailure(int count, bool streamed)
{
    return count < 0 || count > 256 || (!streamed && count == 0) ? "PED_SHADER_GROUP_RANGE" : 0;
}
struct RestoreObservation
{
    uintptr_t playerInfo;
    unsigned activation, model;
    int character, ability;
    bool allowed, carryApplied, appearanceReady;
    RestoreObservation()
        : playerInfo(0), activation(0), model(0), character(-1), ability(-1), allowed(false),
          carryApplied(false), appearanceReady(false)
    {
    }
};
enum RecoveryResult
{
    RECOVERY_PENDING,
    RECOVERY_CONFIRMED,
    RECOVERY_CANCELLED,
    RECOVERY_TIMEOUT
};
class Recovery
{
    bool busy_;
    uintptr_t playerInfo_;
    unsigned activation_, model_;
    int character_, ability_, queued_;
    uint64_t began_;

  public:
    Recovery()
    {
        cancel();
    }
    void cancel()
    {
        busy_ = false;
        playerInfo_ = 0;
        activation_ = model_ = 0;
        character_ = ability_ = queued_ = -1;
        began_ = 0;
    }
    bool busy() const
    {
        return busy_;
    }
    int queued() const
    {
        return queued_;
    }
    bool begin(uintptr_t info, unsigned activation, unsigned model, int character, int ability, int queued,
               uint64_t now)
    {
        if (busy_ || !info || !activation || !model || character < 0 || character > 2 || queued > 2 ||
            queued < -1 || !now)
            return false;
        busy_ = true;
        playerInfo_ = info;
        activation_ = activation;
        model_ = model;
        character_ = character;
        ability_ = ability;
        queued_ = queued;
        began_ = now;
        return true;
    }
    RecoveryResult update(const RestoreObservation& o, uint64_t now)
    {
        if (!busy_)
            return RECOVERY_CANCELLED;
        if (!o.allowed || now < began_ || o.playerInfo != playerInfo_ || o.activation != activation_)
        {
            cancel();
            return RECOVERY_CANCELLED;
        }
        // Native enum is allowed to lag the model command. Model alone is NOT success.
        if (o.model == model_ && o.character == character_ && o.ability == ability_ && o.carryApplied &&
            o.appearanceReady)
        {
            busy_ = false;
            return RECOVERY_CONFIRMED;
        }
        if (now - began_ >= 8000)
        {
            cancel();
            return RECOVERY_TIMEOUT;
        }
        return RECOVERY_PENDING;
    }
};
}
