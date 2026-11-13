#pragma once
#include <stdint.h>
// Emit ONLY the native interaction-menu hold. Native Story/script guards decide
// whether it can open. A denied context expires; never force a script or UI state.
namespace gx_character_menu
{
class Request
{
    uint64_t started_;
    unsigned epoch_;
    bool pending_;

  public:
    Request() : started_(0), epoch_(0), pending_(false) {}
    void reset()
    {
        started_ = 0;
        epoch_ = 0;
        pending_ = false;
    }
    void arm(uint64_t now, unsigned epoch)
    {
        reset();
        if (now && epoch)
        {
            started_ = now;
            epoch_ = epoch;
            pending_ = true;
        }
    }
    bool hold(uint64_t now, unsigned epoch, bool eligible, bool acknowledged)
    {
        if (!pending_)
            return false;
        if (!eligible || epoch != epoch_ || now < started_ || now - started_ > 1200 || acknowledged)
        {
            reset();
            return false;
        }
        return true;
    }
};
}
