#pragma once
#include "../xr/locomotion.h"
namespace gx_wheel
{
// Grip angles are measured in the SAME frozen neutral wheel frame as the model.
// Never measure relative to the already-animated steering wheel (feedback loop).
class Wheel
{
    bool held[2], sample[2];
    double previous[2], travel[2], start[2];
    uint64_t identity;

  public:
    Wheel() : identity(0)
    {
        held[0] = held[1] = sample[0] = sample[1] = false;
        previous[0] = previous[1] = travel[0] = travel[1] = start[0] = start[1] = 0;
    }
    void reset()
    {
        identity = 0;
        held[0] = held[1] = sample[0] = sample[1] = false;
    }
    void suspendSamples()
    {
        sample[0] = sample[1] = false;
    }
    // Buttons can remain available when the hand position is not. Observe an
    // actual release independently; missing/inactive/nonfinite input is not one.
    unsigned releaseFromInput(unsigned active, const float* grip)
    {
        unsigned released = 0;
        if (!grip)
            return released;
        for (unsigned h = 0; h < 2; ++h)
            if (held[h] && (active & (1u << h)) && gx_loco::finite(grip[h]) && grip[h] < .45f)
            {
                held[h] = sample[h] = false;
                released |= 1u << h;
            }
        return released;
    }
    bool contact(unsigned hand, unsigned attached, unsigned active, const float* grip) const
    {
        return hand < 2 && grip && held[hand] && (attached & (1u << hand)) && (active & (1u << hand)) &&
               gx_loco::finite(grip[hand]) && grip[hand] >= .45f;
    }
    bool update(uint64_t vehicle, unsigned valid, const double* angle, const float* grip, double current,
                double maxRadians, double& value, unsigned active = 3)
    {
        if (!vehicle || !angle || !grip || !gx_loco::finite(current) || !gx_loco::finite(maxRadians) ||
            maxRadians < .1)
        {
            reset();
            return false;
        }
        if (vehicle != identity)
        {
            reset();
            identity = vehicle;
        }
        double sum = 0;
        unsigned count = 0;
        for (unsigned h = 0; h < 2; ++h)
        {
            if (!(active & (1u << h)))
            {
                sample[h] = false;
                continue;
            }
            const bool good = (valid & (1u << h)) && gx_loco::finite(angle[h]) && gx_loco::finite(grip[h]);
            if (gx_loco::finite(grip[h]) && grip[h] < (held[h] ? .45 : .65))
            {
                held[h] = sample[h] = false;
                continue;
            }
            // A missing positional sample is NOT a physical grip release. Suspend
            // output, retain contact, and rebase only the angular sample on return.
            if (!good)
            {
                sample[h] = false;
                continue;
            }
            if (!held[h])
            {
                previous[h] = angle[h];
                travel[h] = 0;
                start[h] = current;
                held[h] = true;
            }
            if (sample[h])
                travel[h] += gx_loco::wrap(angle[h] - previous[h]);
            previous[h] = angle[h];
            sample[h] = true;
            // Native car wheel rotation = -normalized steering * maximum angle.
            sum += start[h] - travel[h] / maxRadians;
            ++count;
        }
        if (!count)
            return false;
        value = gx_loco::clamp(sum / count, -1, 1);
        return true;
    }
    bool gripping(unsigned hand) const
    {
        return hand < 2 && held[hand];
    }
};
}
