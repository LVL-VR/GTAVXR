#pragma once
// Main-update intent only. The native adapter separately drains/closes graphics ownership.
namespace gx_loco
{
// Native vehicle FP uses the cinematic director too. A director name alone is
// insufficient to classify an activity/cutscene or discard the anatomical seat.
inline bool scriptedCamera(bool gameplay, bool cinematic, bool seated, bool vehicleFp)
{
    return !gameplay && !(cinematic && seated && vehicleFp);
}
inline bool initialCameraReady(bool directorPresent, bool scripted, bool firstPerson)
{
    return directorPresent && (scripted || firstPerson);
}
class Activation
{
    bool wanted_, fault_, held_;

  public:
    Activation() : wanted_(true), fault_(false), held_(false) {}
    bool wanted() const
    {
        return wanted_ && !fault_;
    }
    bool faulted() const
    {
        return fault_;
    }
    void fault()
    {
        fault_ = true;
        wanted_ = false;
    }
    // Session loss is not permission to reuse handles, nor a permanent device fault.
    // The adapter drains/destroys first; the next explicit F10 requests a NEW session.
    void sessionEnded()
    {
        wanted_ = false;
    }
    // build141 A2: a TRANSIENT pose/focus/compose-input fault ends this session exactly like a session end
    // (drain + destroy first, then the next explicit F10 requests a NEW session). Device/graphics faults latch.
    void transientFault()
    {
        wanted_ = false;
    }
    bool request()
    {
        if (fault_)
            return false;
        wanted_ = !wanted_;
        return true;
    }
    bool poll(bool pressed, bool focused)
    {
        if (!focused)
        {
            held_ = pressed;
            return false;
        }
        const bool edge = pressed && !held_;
        held_ = pressed;
        if (!edge || fault_)
            return false;
        return request();
    }
};
}
