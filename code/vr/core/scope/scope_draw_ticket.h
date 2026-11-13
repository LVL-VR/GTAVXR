#pragma once
// POD identity bridge only. Native lifetime/phase qualification belongs to the
// publisher; pointer VALUES are compared, never followed by this policy.
#include <stdint.h>
namespace gx_scope_ticket
{
struct Key
{
    uintptr_t actor, weapon, object;
    uint64_t epoch, frame, generation, serial;
    unsigned hand, model, objectHandle;
    Key()
        : actor(0), weapon(0), object(0), epoch(0), frame(0), generation(0), serial(0), hand(2), model(0),
          objectHandle(0)
    {
    }
    bool valid() const
    {
        return actor && weapon && object && epoch && frame && generation && serial && hand < 2;
    }
    bool same(const Key& v) const
    {
        return valid() && v.valid() && actor == v.actor && weapon == v.weapon && object == v.object &&
               epoch == v.epoch && frame == v.frame && generation == v.generation && serial == v.serial &&
               hand == v.hand && model == v.model && objectHandle == v.objectHandle;
    }
};
inline bool bind(const Key& published, uintptr_t object, unsigned objectHandle, unsigned model,
                 uint64_t frame, uint64_t generation, Key& out)
{
    out = Key();
    if (!published.valid() || published.object != object || published.objectHandle != objectHandle ||
        published.model != model || published.frame != frame || published.generation != generation)
        return false;
    out = published;
    return true;
}
inline bool draw(const Key& ticket, const Key& current, uint64_t frame, uint64_t generation, unsigned eye,
                 bool worldRoot, bool eligiblePhase)
{
    return eye < 2 && worldRoot && eligiblePhase && ticket.same(current) && ticket.frame == frame &&
           ticket.generation == generation;
}
}
