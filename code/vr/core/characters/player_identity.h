#pragma once
// Update-owned identity tokens only. Never retain a dereferenceable native object.
 
#include <stdint.h>
namespace gx_identity
{
enum Phase
{
    OFFLINE,
    ACTIVE,
    DEAD,
    LOADING,
    TRACKING_LOST,
    RAGDOLL,
    ONLINE
};
enum Change
{
    UNCHANGED = 0,
    ACTOR = 1,
    MODEL = 2,
    SKELETON = 4,
    SESSION = 8,
    STATE = 16,
    SEAT = 32,
    FORCED = 64
};
struct Snapshot
{
    uintptr_t actor, model, skeleton, layout, vehicle;
    unsigned modelHash, bones, layoutSignature, activation, vehicleHash, handMask;
    int seat;
    Phase phase;
    bool human;
    Snapshot()
        : actor(0), model(0), skeleton(0), layout(0), vehicle(0), modelHash(0), bones(0), layoutSignature(0),
          activation(0), vehicleHash(0), handMask(0), seat(-1), phase(OFFLINE), human(false)
    {
    }
    bool usable() const
    {
        return phase == ACTIVE && actor && model && modelHash && skeleton && layout && bones;
    }
};
inline unsigned differences(const Snapshot& a, const Snapshot& b)
{
    unsigned d = 0;
    if (a.actor != b.actor)
        d |= ACTOR;
    if (a.model != b.model || a.modelHash != b.modelHash)
        d |= MODEL;
    if (a.skeleton != b.skeleton || a.layout != b.layout || a.bones != b.bones ||
        a.layoutSignature != b.layoutSignature || a.human != b.human)
        d |= SKELETON;
    if (a.activation != b.activation)
        d |= SESSION;
    if (a.phase != b.phase || a.handMask != b.handMask)
        d |= STATE;
    if (a.vehicle != b.vehicle || a.vehicleHash != b.vehicleHash || a.seat != b.seat)
        d |= SEAT;
    return d;
}
class State
{
    Snapshot current_;
    unsigned epoch_;
    bool ready_;
    void advance()
    {
        if (++epoch_ == 0)
            ++epoch_;
    }

  public:
    State() : epoch_(0), ready_(false) {}
    const Snapshot& current() const
    {
        return current_;
    }
    unsigned epoch() const
    {
        return epoch_;
    }
    bool ready() const
    {
        return ready_ && current_.usable();
    }
    unsigned observe(const Snapshot& next)
    {
        const unsigned changed = differences(current_, next);
        if (changed || !epoch_)
            advance();
        current_ = next;
        ready_ = next.usable();
        return changed;
    }
    void invalidate()
    {
        advance();
        ready_ = false;
    }
    bool matches(const Snapshot& now) const
    {
        return ready() && now.usable() && !differences(current_, now);
    }
    uint64_t poseEpoch(unsigned update) const
    {
        return ready() && current_.human && update ? (uint64_t(epoch_) << 32) | update : 0;
    }
};
}
