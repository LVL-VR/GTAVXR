#pragma once
// Native input thread: F10 alone. F8 and ALL modified F10 combinations stay native.
// No XR calls here; one fresh foreground press queues intent for the update thread.
namespace gx_loco
{
struct HotkeyResult
{
    bool consume, toggle;
};
class VrHotkey
{
    bool held_, reserved_, focused_;

  public:
    VrHotkey() : held_(false), reserved_(false), focused_(false) {}
    HotkeyResult update(bool key, bool modifiers, bool allowed)
    {
        HotkeyResult r = {false, false};
        if (!allowed)
        {
            focused_ = false;
            return r;
        }
        if (!focused_)
        {
            focused_ = true;
            held_ = key;
            reserved_ = key && (reserved_ || !modifiers);
            r.consume = reserved_;
            return r; // require a fresh press after focus returns
        }
        if (!key)
        {
            held_ = false;
            reserved_ = false;
            return r;
        }
        if (!held_ && !modifiers)
        {
            reserved_ = true;
            r.toggle = true;
        }
        held_ = true;
        r.consume = reserved_;
        return r;
    }
};
}
