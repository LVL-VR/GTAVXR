#pragma once
 
#include <stdint.h>
namespace gx_weapon_reload
{
struct Step
{
    bool start, first, empty;
    Step() : start(false), first(false), empty(false) {}
};
class Cycle
{
    uint64_t actor_, weapon_;
    unsigned epoch_;
    bool active_, first_, empty_;

  public:
    Cycle() : actor_(0), weapon_(0), epoch_(0), active_(false), first_(false), empty_(false) {}
    void reset()
    {
        actor_ = weapon_ = 0;
        epoch_ = 0;
        active_ = first_ = empty_ = false;
    }
    Step request(uint64_t actor, unsigned epoch, uint64_t weapon, bool requested, bool empty,
                 bool nativeCanReload, bool looped, bool nativeReloading, bool fireHeld, bool hasLoadedAmmo)
    {
        if (!actor || !epoch || !weapon)
        {
            reset();
            return Step();
        }
        if (actor != actor_ || epoch != epoch_ || weapon != weapon_)
        {
            reset();
            actor_ = actor;
            epoch_ = epoch;
            weapon_ = weapon;
        }
        // Do not replace an ongoing native timer or cancel its transaction.
        if (nativeReloading)
            return Step();
        if (!nativeCanReload)
        {
            active_ = first_ = empty_ = false;
            return Step();
        }
        if (active_ && looped && fireHeld && hasLoadedAmmo)
        {
            active_ = first_ = empty_ = false;
            return Step();
        }
        if (active_ && !looped && !first_)
        {
            active_ = first_ = empty_ = false;
        }
        if (!active_ && (requested || empty))
        {
            active_ = true;
            first_ = true;
            empty_ = empty;
        }
        if (!active_ || (!looped && !first_))
        {
            active_ = false;
            return Step();
        }
        Step out;
        out.start = true;
        out.first = first_;
        out.empty = empty_;
        return out;
    }
    bool started(uint64_t actor, unsigned epoch, uint64_t weapon, bool success)
    {
        if (actor != actor_ || epoch != epoch_ || weapon != weapon_ || !active_)
            return false;
        if (success)
            first_ = false;
        return true; // unavailable native clip retries before state mutation
    }
};
}
