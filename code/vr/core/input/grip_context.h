#pragma once
#include "../core/input.h"
#include <math.h>
#include <float.h>
namespace gx_grip
{
inline void padInput(GxInput& i, bool gameplay, bool seated)
{
    // Holster wheel is injected AFTER mapping. On foot neither tracked grip
    // becomes a generic pad button; C4 uses the original semantic grip edge.
    // Vehicles/pause and physical Xbox/keyboard snapshots remain unchanged.
    if (gameplay && !seated)
        i.grip[0] = i.grip[1] = 0;
}
class Detonator
{
    bool held_;

  public:
    Detonator() : held_(false) {}
    bool update(const GxInput& i, bool eligible)
    {
        if (i.size != sizeof(i) || i.version != 1 || !(i.active_hands & 1) || !_finite(i.grip[0]))
        {
            held_ = false;
            return false;
        }
        const bool now = i.grip[0] >= (held_ ? .45f : .65f), edge = now && !held_;
        held_ = now;
        return edge && eligible;
    }
};
}
