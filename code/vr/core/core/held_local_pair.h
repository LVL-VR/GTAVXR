#pragma once
// Independently authored GTAVXR exact pair policy. Addresses are opaque IDs,
// never dereferenced here. A miss preserves original native collision behavior.
#include <stdint.h>
#include "../physics/physics_contact.h"
namespace gx_held_local_pair
{
// Preserve the actual owner release threshold; invalid raw input only revokes.
inline bool releasedOrInvalid(const gx_physics::GripLatch& grip, float value)
{
    return !gx_physics::finite(value) || grip.released(value);
}
enum Kind
{
    NONE,
    CAPSULE,
    MOVABLE_OBJECT
};
struct Body
{
    uintptr_t instance, entity, archetype, whole, part;
    unsigned level, generation, component, lod, nativeClass, kind;
    Body()
        : instance(0), entity(0), archetype(0), whole(0), part(0), level(65535), generation(0), component(0),
          lod(0), nativeClass(0), kind(NONE)
    {
    }
    bool valid() const
    {
        return instance && entity && archetype && whole && part && level < 65535 && generation <= 65535 &&
               component <= 65535 && lod <= 65535 &&
               (kind == MOVABLE_OBJECT || (kind == CAPSULE && component == 0 && part == whole));
    }
    bool same(const Body& o) const
    {
        return instance == o.instance && entity == o.entity && archetype == o.archetype && whole == o.whole &&
               part == o.part && level == o.level && generation == o.generation && component == o.component &&
               lod == o.lod && nativeClass == o.nativeClass && kind == o.kind;
    }
};
struct Snapshot
{
    uint64_t tick, frame, updateGeneration, inputSequence;
    unsigned epoch, heldMask;
    Body local, held[2];
    Snapshot() : tick(0), frame(0), updateGeneration(0), inputSequence(0), epoch(0), heldMask(0) {}
    bool valid() const
    {
        if (!tick || !frame || !updateGeneration || !epoch || !heldMask || (heldMask & ~3u) ||
            !local.valid() || local.kind != CAPSULE)
            return false;
        for (unsigned h = 0; h < 2; ++h)
            if (heldMask & (1u << h))
            {
                if (!held[h].valid() || held[h].kind != MOVABLE_OBJECT ||
                    held[h].instance == local.instance || held[h].entity == local.entity)
                    return false;
            }
        return true;
    }
};
inline bool fresh(const Snapshot& s, uint64_t now)
{
    return s.valid() && now >= s.tick && now - s.tick <= 100;
}
// Same-pose native UPDATE/input changes may ONLY revoke existing owners.
// No reacquisition or lease-time refresh without fresh native body validation.
inline Snapshot revoked(const Snapshot& s, uint64_t now, unsigned epoch, uint64_t frame,
                        uint64_t updateGeneration, uint64_t inputSequence, unsigned allowedMask,
                        unsigned releasedMask)
{
    if (!fresh(s, now) || s.epoch != epoch || s.frame != frame || s.updateGeneration != updateGeneration)
        return Snapshot();
    Snapshot out = s;
    out.heldMask &= allowedMask & ~releasedMask & 3u;
    out.inputSequence = inputSequence;
    for (unsigned h = 0; h < 2; ++h)
        if (!(out.heldMask & (1u << h)))
            out.held[h] = Body();
    return out.valid() ? out : Snapshot();
}
inline bool ignores(const Snapshot& s, uint64_t now, const Body& a, const Body& b)
{
    if (!fresh(s, now) || !a.valid() || !b.valid())
        return false;
    const Body* object = 0;
    if (a.kind == CAPSULE && a.same(s.local) && b.kind == MOVABLE_OBJECT)
        object = &b;
    else if (b.kind == CAPSULE && b.same(s.local) && a.kind == MOVABLE_OBJECT)
        object = &a;
    else
        return false;
    for (unsigned h = 0; h < 2; ++h)
        if ((s.heldMask & (1u << h)) && object->same(s.held[h]))
            return true;
    return false;
}
}
