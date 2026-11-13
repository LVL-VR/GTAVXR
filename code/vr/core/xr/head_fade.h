#pragma once
 
// darkens with the depth, so a wall cannot be seen through. depth = metres from the first surface between the player's
// capsule axis (at eye height) and the VR eye to the eye; < 0 = no measurement this frame. The alpha reaches the bridge
 
 
#include "../core/bridge.h"
typedef int(GX_CALL* GxSetViewFade)(void*, float);
namespace gx_head_fade
{
const double START_M = .02, FULL_M = .12, CLEAR_PER_S = 3.0, MAX_REACH_M = 1.5;
inline double target(double depth)
{
    if (!(depth > START_M))
        return 0;
    if (depth >= FULL_M)
        return 1;
    return (depth - START_M) / (FULL_M - START_M);
}
// darkens at once (never a frame of see-through), clears at CLEAR_PER_S
class Fade
{
    double alpha_, at_;
    bool started_;

  public:
    Fade() : alpha_(0), at_(0), started_(false) {}
    void reset()
    {
        alpha_ = 0;
        started_ = false;
    }
    double alpha() const
    {
        return alpha_;
    }
    double update(double depth, double now)
    {
        const double t = target(depth);
        double dt = started_ && now > at_ ? now - at_ : 0;
        if (dt > .25)
            dt = .25;
        at_ = now;
        started_ = true;
        if (t >= alpha_)
            alpha_ = t;
        else
        {
            alpha_ -= CLEAR_PER_S * dt;
            if (alpha_ < t)
                alpha_ = t;
        }
        return alpha_;
    }
};
} // namespace gx_head_fade
