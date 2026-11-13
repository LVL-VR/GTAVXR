#pragma once
// Build156 vehicle-part policy. Pure C++ accepted by modern MSVC and v110.
 
// Release returns ratio/second for native joint velocity. GAS release keeps the existing rule.
#include <stdint.h>
namespace gx_vdoor
{
enum Kind
{
    DOOR,
    GAS,
    SLIDE
};
enum Lock
{
    OPEN,
    BREAK_IN,
    LOCKED
};
enum Event
{
    NO_EVENT,
    EV_GRAB,
    EV_UNLOCK,
    EV_LOCKED,
    EV_LATCH,
    EV_SLAM,
    EV_STAY,
    EV_GAS_UP,
    EV_GAS_DOWN,
    EV_CANCEL,
    EV_JACK,
    EVENT_COUNT
};
inline double unit(double x)
{
    return x < 0 ? 0 : (x > 1 ? 1 : x);
}
enum Route
{
    IGNORE_PART,
    HANDLE_PART,
    ENTER_SEAT
};
 
inline Route route(bool inside, bool occupied, bool seat, bool slowEnough, double roadSpeedSquared)
{
    if (inside)
        return HANDLE_PART;
    if (!slowEnough)
        return IGNORE_PART;
    if (seat && (occupied || roadSpeedSquared > 1.))
        return ENTER_SEAT;
    return roadSpeedSquared <= 1. ? HANDLE_PART : IGNORE_PART;
}
struct Input
{
    bool live, tracked;
    uint64_t part;
    Kind kind;
    Lock lock;
    bool nearPart, jack;
    double grip, ratio, handRatio, now;
    Input()
        : live(false), tracked(false), part(0), kind(DOOR), lock(OPEN), nearPart(false), jack(false), grip(0),
          ratio(0), handRatio(0), now(0)
    {
    }
};
struct Output
{
    Event event;
    bool held, drive, place, release, latch, unlock, capture, claim, graced, jack;
    double target, speed;
    uint64_t part;
    Output()
        : event(NO_EVENT), held(false), drive(false), place(false), release(false), latch(false),
          unlock(false), capture(false), claim(false), graced(false), jack(false), target(0), speed(0),
          part(0)
    {
    }
};
class Door
{
    bool held_, gripDown_, gripNeutral_;
    uint64_t part_;
    Kind kind_;
    double ownedAt_, target_, prevAt_, speed_;

  public:
    Door()
    {
        reset();
    }
    void reset()
    {
        held_ = gripDown_ = gripNeutral_ = false;
        part_ = 0;
        kind_ = DOOR;
        ownedAt_ = target_ = prevAt_ = speed_ = 0;
    }
    static double HOLD_GRACE()
    {
        return .25;
    }
    static double SHUT()
    {
        return .02;
    }
    static double SLAM_SPEED()
    {
        return 2.;
    }
    static double SLAM_ZONE()
    {
        return .35;
    }
    bool held() const
    {
        return held_;
    }
    uint64_t part() const
    {
        return part_;
    }
    Output update(const Input& i)
    {
        Output o;
        if (!i.live)
        {
            if (held_)
            {
                o.event = EV_CANCEL;
                o.release = true;
                o.target = target_;
                o.part = part_;
            }
            const bool down = gripDown_;
            reset();
            gripDown_ = down;
            return o;
        }
        const bool wasTracked = i.tracked && i.now - ownedAt_ <= HOLD_GRACE();
        if (i.tracked)
            ownedAt_ = i.now;
        const bool graced = !i.tracked && held_ && gripDown_ && i.now - ownedAt_ <= HOLD_GRACE();
        o.graced = graced;
        if (!i.tracked && !graced)
        {
            gripDown_ = false;
            gripNeutral_ = false;
        }
        else if (i.grip < .3)
        {
            gripNeutral_ = true;
            gripDown_ = false;
        }
        const bool edge = i.tracked && gripNeutral_ && !gripDown_ && i.grip >= .7;
        if (edge)
        {
            gripDown_ = true;
            gripNeutral_ = false;
        }
        const bool gripHeld = (i.tracked || graced) && gripDown_ && i.grip >= .3;
        bool fresh = false;
        if (!held_ && edge && i.nearPart && i.part)
        {
            if (i.jack)
            {
                o.event = EV_JACK;
                o.jack = true;
                o.part = i.part;
            }
            else if (i.lock == LOCKED)
                o.event = EV_LOCKED;
            else
            {
                held_ = true;
                fresh = true;
                part_ = i.part;
                kind_ = i.kind;
                target_ = unit(i.ratio);
                prevAt_ = i.now;
                speed_ = 0;
                o.capture = true;
                if (i.lock == BREAK_IN)
                {
                    o.unlock = true;
                    o.event = EV_UNLOCK;
                }
                else
                    o.event = EV_GRAB;
            }
        }
        if (held_ && !fresh)
        {
            if (i.tracked && gripHeld)
            {
                const double t = unit(i.handRatio), dt = i.now - prevAt_;
                speed_ = wasTracked && dt > 1e-4 && dt <= HOLD_GRACE() ? (t - target_) / dt : 0;
                prevAt_ = i.now;
                target_ = t;
            }
            else if (!i.tracked)
                speed_ = 0;
            if (!gripHeld)
            {
                held_ = false;
                if (kind_ == GAS)
                {
                    if (target_ >= .5)
                    {
                        target_ = 1;
                        o.drive = true;
                        o.event = EV_GAS_UP;
                    }
                    else
                    {
                        target_ = 0;
                        o.latch = true;
                        o.event = EV_GAS_DOWN;
                    }
                }
                else if (target_ <= SHUT() || (speed_ <= -SLAM_SPEED() && target_ <= SLAM_ZONE()))
                {
                    o.latch = true;
                    o.event = target_ <= SHUT() ? EV_LATCH : EV_SLAM;
                    target_ = 0;
                }
                else
                {
                    o.release = true;
                    o.speed = speed_;
                    o.event = EV_STAY;
                }
            }
        }
        if (held_)
        {
            o.place = o.drive = true;
            o.speed = speed_;
        }
        o.held = held_;
        if (held_ || o.drive || o.latch || o.release)
            o.part = part_;
        o.target = target_;
        o.claim = held_;
        return o;
    }
};
// An entering fast fist or gun butt breaks glass once, with a cooldown.
class Smash
{
    double lastAt_;
    bool inside_;

  public:
    Smash() : lastAt_(-10), inside_(false) {}
    // m/s into the glass (fist or gun butt)
    static double SPEED()
    {
        return 2.;
    }
    static double COOLDOWN()
    {
        return .4;
    }
    // inside: the hand (or the gun's front) is at an intact side window; closing: its speed into the glass (m/s, + = into it)
    bool update(bool live, bool inside, double closing, double now)
    {
        const bool entering = inside && !inside_;
        inside_ = inside;
        if (!live || !inside)
            return false;
        if (entering && closing >= SPEED() && now - lastAt_ >= COOLDOWN())
        {
            lastAt_ = now;
            return true;
        }
        return false;
    }
};
// Shared input ownership decision, evaluated against the fresh grip snapshot.
inline unsigned windowStickMask(unsigned panels, unsigned active, double leftGrip, double rightGrip,
                                bool live)
{
    return live && (((panels & active & 1u) && leftGrip >= .3) || ((panels & active & 2u) && rightGrip >= .3))
               ? 3u
               : 0u;
}
// Window-switch contact and binary flick fallback. Either contact owns both stick Y axes.
enum WindowEvent
{
    WIN_NONE,
    WIN_GRAB,
    WIN_LET_GO
};
class Window
{
    bool held_, down_, neutral_;

  public:
    Window() : held_(false), down_(false), neutral_(false) {}
    bool held() const
    {
        return held_;
    }
    WindowEvent update(bool live, bool tracked, bool nearPanel, double grip)
    {
        if (!live || !tracked)
        {
            const bool was = held_;
            held_ = false;
            neutral_ = false;
            return was ? WIN_LET_GO : WIN_NONE;
        }
        if (grip < .3)
        {
            neutral_ = true;
            down_ = false;
        }
        const bool edge = neutral_ && !down_ && grip >= .7;
        if (edge)
        {
            down_ = true;
            neutral_ = false;
        }
        if (held_ && grip < .3)
        {
            held_ = false;
            return WIN_LET_GO;
        }
        if (!held_ && edge && nearPanel)
        {
            held_ = true;
            return WIN_GRAB;
        }
        return WIN_NONE;
    }
};
// A flick is rearmed at neutral. Both sticks work regardless of the gripping hand.
class WindowSticks
{
    bool armed_[2];

  public:
    WindowSticks()
    {
        reset();
    }
    void reset()
    {
        armed_[0] = armed_[1] = false;
    }
    int update(unsigned side, bool held, double y)
    {
        if (side > 1)
            return 0;
        if (!held)
        {
            armed_[side] = false;
            return 0;
        }
        if (y > -.25 && y < .25)
        {
            armed_[side] = true;
            return 0;
        }
        if (!armed_[side])
            return 0;
        if (y >= .65)
        {
            armed_[side] = false;
            return 1;
        }
        if (y <= -.65)
        {
            armed_[side] = false;
            return -1;
        }
        return 0;
    }
};
}
