#pragma once
 
// correspondiente ... presiono grip ... y luego martillo abajo, esto permitiria disparar de forma realista con dos revolvers uno
// en cada mano"; "no queremos romper el dual wielding solo por tener realista"). Per-hand thumb hammer:
//   Gesture: the gun hand HOLDS its grip, then moves its stick DOWN (not a click): one cock edge. The stick must pass through
//            neutral while the grip is held, so a stick already down when the grip is pressed does nothing.
 
//            automatic reload stays). A mandatory hammer (Navy, Marksman) blocks fire until cocked; an optional one (Revolver,
//            Mk II, Double Action) is only shown back. Every native shot lets the hammer fall. Native ammo is never touched here.
 
#include <stdint.h>
#include <math.h>
namespace gx_hammer
{
class Gesture
{
    bool held_, armed_;

  public:
    Gesture() : held_(false), armed_(false) {}
    static float GRIP_ON()
    {
        return .65f;
    }
    static float GRIP_OFF()
    {
        return .45f;
    }
    static float STICK_DOWN()
    {
        return -.7f;
    }
    static float STICK_NEUTRAL()
    {
        return -.35f;
    }
    void reset()
    {
        held_ = armed_ = false;
    }
    bool held() const
    {
        return held_;
    }
    // grip 0..1, stickY -1 (down) .. +1 (up). Returns true once per stick-down stroke made while the grip is held.
    bool update(bool tracked, float grip, float stickY)
    {
        if (!tracked || !(grip == grip) || !(stickY == stickY))
        {
            reset();
            return false;
        }
        held_ = grip >= (held_ ? GRIP_OFF() : GRIP_ON());
        if (!held_)
        {
            armed_ = false;
            return false;
        }
        if (stickY > STICK_NEUTRAL())
            armed_ = true;
        if (armed_ && stickY <= STICK_DOWN())
        {
            armed_ = false;
            return true;
        }
        return false;
    }
};
enum Event
{
    NO_EVENT,
    EV_COCK,
    EV_SHOT,
    EV_DRY,
    EVENT_COUNT
};
struct Input
{
    bool live;
    uint64_t weapon;
    int clip;
    bool mandatory;
    bool cockEdge;
    bool triggerPull;
    bool nativeReloading;
    Input()
        : live(false), weapon(0), clip(0), mandatory(false), cockEdge(false), triggerPull(false),
          nativeReloading(false)
    {
    }
};
struct Output
{
    Event event;
    bool hammerBack, blockFire;
    Output() : event(NO_EVENT), hammerBack(false), blockFire(false) {}
};
class Machine
{
    bool started_, back_, triggerDown_;
    uint64_t weapon_;
    int lastClip_;

  public:
    Machine()
    {
        reset();
    }
    void reset()
    {
        started_ = back_ = false;
        triggerDown_ = true;
        weapon_ = 0;
        lastClip_ = 0;
    }
    bool hammerBack() const
    {
        return back_;
    }
    Output update(const Input& i)
    {
        Output o;
        if (!i.live || !i.weapon || (weapon_ && i.weapon != weapon_))
        {
            reset();
            return o;
        }
        if (!started_)
        {
            started_ = true;
            weapon_ = i.weapon;
            lastClip_ = i.clip;
            triggerDown_ = i.triggerPull;
            back_ = false;
        }
        Event e = NO_EVENT;
        const bool triggerEdge = i.triggerPull && !triggerDown_;
        triggerDown_ = i.triggerPull;
        if (i.clip < lastClip_ && !i.nativeReloading)
        {
            back_ = false;
            e = EV_SHOT;
        } // a native shot: the hammer falls
        lastClip_ = i.clip;
        if (i.cockEdge && !back_ && !i.nativeReloading)
        {
            back_ = true;
            if (e == NO_EVENT)
                e = EV_COCK;
        }
        else if (triggerEdge && i.mandatory && !back_ && !i.nativeReloading && e == NO_EVENT)
            e = EV_DRY; // single action, not cocked: click
        o.event = e;
        o.hammerBack = back_;
        o.blockFire = i.mandatory && !back_ && !i.nativeReloading;
        return o;
    }
};
}
