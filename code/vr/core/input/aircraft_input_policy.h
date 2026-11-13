#pragma once
#include "../core/input.h"
namespace gx_air_input
{
struct Intent
{
    bool fire, cycle, reserveTrigger, reserveA, reserveX;
    Intent() : fire(false), cycle(false), reserveTrigger(false), reserveA(false), reserveX(false) {}
};
inline Intent intent(const GxInput& i, bool eligible, bool throttleOwned, bool armed)
{
    Intent o;
    if (!eligible)
        return o;
    o.reserveTrigger = throttleOwned;
    o.reserveA = armed;
    o.reserveX = armed;
    o.fire = armed && (throttleOwned ? i.trigger[1] >= .55f : (i.buttons & GX_A) != 0);
    o.cycle = armed && (i.buttons & GX_X) != 0;
    return o;
}
class TriggerHandoff
{
    bool initialized, prior, draining;

  public:
    TriggerHandoff() : initialized(false), prior(false), draining(false) {}
    bool update(bool eligible, bool throttleOwned, float trigger)
    {
        if (!eligible)
        {
            initialized = draining = false;
            return false;
        }
        if (initialized && prior != throttleOwned && trigger >= .2f)
            draining = true;
        initialized = true;
        prior = throttleOwned;
        if (trigger < .15f)
            draining = false;
        return draining;
    }
};
}
