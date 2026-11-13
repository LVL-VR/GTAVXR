#pragma once
// An unchorded click requests its native action on release. No timer/window.
// Cancellation is sticky through the whole press; recovery requires neutral.
namespace gx_character
{
class SingleClickIntent
{
    bool barrier_, pending_, cancelled_;

  public:
    SingleClickIntent()
    {
        reset();
    }
    void reset()
    {
        barrier_ = true;
        pending_ = cancelled_ = false;
    }
    bool pending() const
    {
        return pending_;
    }
    bool update(bool down, bool cancel, bool eligible)
    {
        if (!eligible)
        {
            reset();
            return false;
        }
        if (barrier_)
        {
            if (!down)
                barrier_ = false;
            return false;
        }
        if (down)
        {
            if (!pending_)
            {
                pending_ = true;
                cancelled_ = false;
            }
            cancelled_ = cancelled_ || cancel;
            return false;
        }
        if (!pending_)
            return false;
        const bool request = !cancelled_ && !cancel;
        pending_ = cancelled_ = false;
        return request;
    }
};
}
