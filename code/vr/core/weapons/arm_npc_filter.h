#ifndef GTAVXR_ARM_NPC_FILTER_H
#define GTAVXR_ARM_NPC_FILTER_H

#include <stdint.h>

// Original CPU policy model, not a game patch. IDs are opaque equality tokens;
// no pointer is dereferenced, no game headers, allocation, clock or atomics.
namespace gx_arm_npc
{

enum InstanceKind
{
    OTHER_INSTANCE,
    PED_CAPSULE,
    PED_RAGDOLL,
    VEHICLE,
    OBJECT,
    DOOR,
    STATIC_WORLD
};

 
// PH_INST_PED == ped.GetAnimatedInst(), or
// PH_INST_FRAG_PED == ped.GetRagdollInst(). Flags alone cannot classify a ped.
// Copy these fields on the simulation caller, not in solver worker loops.
struct Instance
{
    uintptr_t id;
    uintptr_t entity;
    InstanceKind kind;
    bool inLevel;
    bool levelActive;
    bool usingRagdoll;          
    bool physActivate;         // GetRagdollState() == PHYS_ACTIVATE
    bool keepInactiveContacts;  
    bool dead;                 // diagnostic only; never changes this policy
    bool inVehicle;            // diagnostic only; native seat policy still runs
};

struct Pair
{
    uintptr_t publishedArm; // one atomic load of nativeBodyContactInstance
    Instance a;
    Instance b;
};

inline bool isPed(const Instance& instance)
{
    return instance.kind == PED_CAPSULE || instance.kind == PED_RAGDOLL;
}

inline bool isOwnedArm(const Instance& instance, uintptr_t publishedArm)
{
    // The private publication is made only after the arm-only mask lease.
    // Live state guards also protect natural player ragdoll during revocation.
    return publishedArm != 0 && instance.id == publishedArm && instance.entity != 0 &&
           instance.kind == PED_RAGDOLL && instance.inLevel && !instance.levelActive &&
           !instance.usingRagdoll && !instance.physActivate && instance.keepInactiveContacts;
}

inline bool vetoOrdered(const Instance& arm, const Instance& target, uintptr_t publishedArm)
{
    if (!isOwnedArm(arm, publishedArm) || target.id == 0 || target.id == arm.id || target.entity == 0 ||
        target.entity == arm.entity || !target.inLevel || !isPed(target))
    {
        return false;
    }

     
    // An activation request without active physics is still animated here.
    const bool committedActivation = target.kind == PED_RAGDOLL && target.levelActive && target.physActivate;
    return !target.usingRagdoll && !committedActivation;
}

// true = additional ShouldFindImpacts veto; false = continue all native gates.
// false never forces a native pair to be admitted.
inline bool veto(const Pair& pair)
{
    return vetoOrdered(pair.a, pair.b, pair.publishedArm) || vetoOrdered(pair.b, pair.a, pair.publishedArm);
}

} // namespace gx_arm_npc142

#endif
