#pragma once
 
// optional single-action hammer. One machine for:
//   RPG, Homing Launcher, Firework launcher: a rocket from the torso, aligned with the tube and pushed in at the muzzle;
//   Musket: a ball (substitute model) pushed in at the muzzle, then the hammer cocked (grip + stick down, or the free hand);
//   Grenade Launcher: open the action (free hand swings it), grenades one by one into the drum, close it;
//   Snowball launcher: open the action, one snowball pack (a whole native load), close it.
// Rules: a shot needs the action closed, a cocked hammer (when it has one) and a loaded round; a hammer falls with every shot;
 
 
#include <stdint.h>
namespace gx_muzzle
{
enum Round
{
    ROUND_NONE,
    ROUND_HELD,
    ROUND_ENTERING,
    ROUND_SEATING
};
enum Event
{
    NO_EVENT,
    EV_SHOT,
    EV_OPEN,
    EV_CLOSE,
    EV_ROUND_GRAB,
    EV_ROUND_ALIGN,
    EV_ROUND_IN,
    EV_ROUND_SEATED,
    EV_ROUND_DROP,
    EV_COCK,
    EV_DRY,
    EV_RELOADED,
    EV_CANCEL,
    EV_GRAB,
    EV_LET_GO,
    EVENT_COUNT
};
struct Input
{
    bool live;
    uint64_t weapon;
    int clip;
    bool canLoad, nativeReloading, freeOwned;
    double freeGrip;
    bool triggerPull;
    double now;
    // build156: the free hand pose is valid (granted or not)
    bool freeTracked;
    // the held round vs the port: 0 mouth .. 1 seated
    bool atTorso;
    bool atPort, onPort;
    double portDepth;
    bool roundDirect;
    // the action (GL / snowball): free hand at it; its angle 0 .. 1
    bool hasAction;
    bool nearAction;
    double action01;
    // Musket hammer
    bool needsHammer;
    bool thumbCock;
    bool nearHammer;
    double hammerPull01;
    Input()
        : live(false), weapon(0), clip(0), canLoad(false), nativeReloading(false), freeOwned(false),
          freeTracked(false), freeGrip(0), triggerPull(false), now(0), atTorso(false), atPort(false),
          onPort(false), portDepth(0), roundDirect(false), hasAction(false), nearAction(false), action01(0),
          needsHammer(false), thumbCock(false), nearHammer(false), hammerPull01(0)
    {
    }
};
struct Output
{
    Event event;
    double open01;
    bool closed, cocked, heldAction, heldHammer, capture, blockFire, claimFree, graced, cancelled;
    Round round;
    bool holdRound, roundEntering, loadRound;
    bool glowRound, glowAction, glowHammer;
    Output()
        : event(NO_EVENT), open01(0), closed(true), cocked(true), heldAction(false), heldHammer(false),
          capture(false), blockFire(false), claimFree(false), graced(false), cancelled(false),
          round(ROUND_NONE), holdRound(false), roundEntering(false), loadRound(false), glowRound(false),
          glowAction(false), glowHammer(false)
    {
    }
};
class Machine
{
    bool started_, closed_, cocked_, heldAction_, heldHammer_, gripDown_, gripNeutral_, triggerDown_,
        sawReload_, thumbDown_;
    double open01_, ownedAt_, roundAt_, entry_;
    uint64_t weapon_;
    int lastClip_;
    Round round_;

  public:
    Machine()
    {
        reset();
    }
    void reset()
    {
        started_ = false;
        closed_ = true;
        cocked_ = true;
        heldAction_ = heldHammer_ = gripDown_ = gripNeutral_ = sawReload_ = thumbDown_ = false;
        triggerDown_ = true;
        open01_ = ownedAt_ = roundAt_ = entry_ = 0;
        weapon_ = 0;
        lastClip_ = 0;
        round_ = ROUND_NONE;
    }
    static double HOLD_GRACE()
    {
        return .25;
    }
    static double ENTRY_MAX()
    {
        return .4;
    }
    Round round() const
    {
        return round_;
    }
    bool heldAction() const
    {
        return heldAction_;
    }
    bool heldHammer() const
    {
        return heldHammer_;
    }
    bool closed() const
    {
        return closed_;
    }
    Output update(const Input& i)
    {
        Output o;
        if (!i.live || !i.weapon || (weapon_ && i.weapon != weapon_))
        {
            o.cancelled = started_ && (heldAction_ || heldHammer_ || round_ != ROUND_NONE || !closed_);
            if (o.cancelled)
                o.event = EV_CANCEL;
            reset();
            return o;
        }
        if (!started_)
        {
            started_ = true;
            weapon_ = i.weapon;
            lastClip_ = i.clip;
            triggerDown_ = i.triggerPull;
            cocked_ = !i.needsHammer;
        }
        Event e = NO_EVENT;
        if (i.nativeReloading)
        {
            sawReload_ = true;
            heldAction_ = heldHammer_ = false;
            round_ = ROUND_NONE;
        }
        else if (sawReload_)
        {
            sawReload_ = false;
            closed_ = true;
            open01_ = 0;
            cocked_ = true;
            e = EV_RELOADED;
        }
        if (i.clip < lastClip_ && !i.nativeReloading)
        {
            if (i.needsHammer)
                cocked_ = false;
            if (e == NO_EVENT)
                e = EV_SHOT;
        }
        lastClip_ = i.clip;
        const bool triggerEdge = i.triggerPull && !triggerDown_;
        triggerDown_ = i.triggerPull;
        if (i.freeOwned)
            ownedAt_ = i.now;
        const bool busy = heldAction_ || heldHammer_ || round_ == ROUND_HELD || round_ == ROUND_ENTERING;
        const bool graced = !i.freeOwned && gripDown_ && busy && i.now - ownedAt_ <= HOLD_GRACE();
        o.graced = graced;
        // build156 grip class fix (155 worn: no slide / pump grab): the release is seen whenever the free hand is TRACKED (not only
        // while already granted), a press near a part is a grab at once, and the grab opens the grace window so the arbiter can
        // grant the hand on the next update (claimFree is set by the grab); no grant within the grace = dropped as before.
        const bool freeHandAvailable = i.freeOwned || i.freeTracked;
        if (!freeHandAvailable && !graced)
        {
            gripDown_ = false;
            gripNeutral_ = false;
        }
        else if (i.freeGrip < .3)
        {
            gripNeutral_ = true;
            gripDown_ = false;
        }
        bool gripEdge = freeHandAvailable && (gripNeutral_ || (i.nearAction || i.nearHammer)) && !gripDown_ &&
                        i.freeGrip >= .7 && (i.nearAction || i.nearHammer || (i.atTorso && i.canLoad));
        if (gripEdge)
        {
            gripDown_ = true;
            gripNeutral_ = false;
            if (!i.freeOwned)
                ownedAt_ = i.now;
        }
        const bool gripHeld = (i.freeOwned || graced) && gripDown_ && i.freeGrip >= .3;
        bool load = false;
        // build154 class fix: the glue measures a held part relative to its capture, which it stores AFTER this update; on the grab
        // update itself those measurements still belong to the previous capture, so a part grabbed this update is not moved by them.
        bool fresh = false;
        if (!i.nativeReloading)
        {
            // ---- the action (GL drum / snowball breach): the free hand swings it open and shut
            if (i.hasAction)
            {
                if (!heldAction_ && !heldHammer_ && round_ == ROUND_NONE && gripEdge && i.nearAction)
                {
                    heldAction_ = true;
                    fresh = true;
                    o.capture = true;
                    gripEdge = false;
                    if (e == NO_EVENT)
                        e = EV_GRAB;
                }
                if (heldAction_ && !fresh)
                {
                    if (!gripHeld)
                    {
                        heldAction_ = false;
                        if (e == NO_EVENT)
                            e = EV_LET_GO;
                    }
                    else if (i.freeOwned)
                    {
                        const double a = i.action01 < 0 ? 0 : (i.action01 > 1 ? 1 : i.action01);
                        if (closed_)
                        {
                            if (a >= .15)
                            {
                                closed_ = false;
                                open01_ = a;
                                if (e == NO_EVENT)
                                    e = EV_OPEN;
                            }
                        }
                        else
                        {
                            open01_ = a;
                            if (a <= .05)
                            {
                                closed_ = true;
                                open01_ = 0;
                                heldAction_ = false;
                                if (e == NO_EVENT)
                                    e = EV_CLOSE;
                            }
                        }
                    }
                }
            }
            // ---- the hammer (Musket): the gun hand's thumb, or the free hand pulling it back
            if (i.needsHammer && closed_ && !cocked_)
            {
                if (i.thumbCock && !thumbDown_)
                {
                    cocked_ = true;
                    if (e == NO_EVENT)
                        e = EV_COCK;
                }
                else if (!heldAction_ && !heldHammer_ && round_ == ROUND_NONE && gripEdge && i.nearHammer)
                {
                    heldHammer_ = true;
                    fresh = true;
                    o.capture = true;
                    gripEdge = false;
                }
            }
            if (heldHammer_ && !fresh)
            {
                if (!gripHeld)
                    heldHammer_ = false;
                else if (i.freeOwned && i.hammerPull01 >= .9)
                {
                    cocked_ = true;
                    heldHammer_ = false;
                    if (e == NO_EVENT)
                        e = EV_COCK;
                }
            }
            // ---- one round into the port
            const bool portOpen = !i.hasAction || (!closed_ && open01_ >= .8);
            switch (round_)
            {
            case ROUND_NONE:
                if (!heldAction_ && !heldHammer_ && gripEdge && i.atTorso && i.canLoad && portOpen)
                {
                    round_ = ROUND_HELD;
                    roundAt_ = i.now;
                    gripEdge = false;
                    if (e == NO_EVENT)
                        e = EV_ROUND_GRAB;
                }
                break;
            case ROUND_HELD:
                if (!gripHeld)
                {
                    round_ = ROUND_NONE;
                    if (e == NO_EVENT)
                        e = EV_ROUND_DROP;
                }
                else if (i.freeOwned && portOpen && i.atPort && i.portDepth <= ENTRY_MAX())
                {
                    round_ = ROUND_ENTERING;
                    entry_ = i.portDepth;
                    if (e == NO_EVENT)
                        e = EV_ROUND_ALIGN;
                }
                break;
            case ROUND_ENTERING:
                // A complete tracked insertion also seats when grip is released in this sample.
                if (i.freeOwned && portOpen && i.onPort && i.portDepth >= 1 && entry_ <= ENTRY_MAX())
                {
                    if (i.canLoad)
                    {
                        round_ = ROUND_SEATING;
                        roundAt_ = i.now;
                        load = true;
                        if (e == NO_EVENT)
                            e = EV_ROUND_IN;
                    }
                    else
                    {
                        round_ = ROUND_NONE;
                        if (e == NO_EVENT)
                            e = EV_ROUND_DROP;
                    }
                }
                else if (!gripHeld)
                {
                    round_ = ROUND_NONE;
                    if (e == NO_EVENT)
                        e = EV_ROUND_DROP;
                }
                // graced: hold, no edge
                else if (!i.freeOwned)
                    break;
                else if (!portOpen || !i.onPort)
                    round_ = ROUND_HELD;
                break;
            case ROUND_SEATING:
                if (i.roundDirect || i.now - roundAt_ > 1.0)
                {
                    if (i.roundDirect && e == NO_EVENT)
                        e = EV_ROUND_SEATED;
                    round_ = ROUND_NONE;
                }
                break;
            }
        }
        thumbDown_ = i.thumbCock;
        o.loadRound = load;
        const bool canFire = closed_ && cocked_ && i.clip > 0 && !heldAction_ && round_ != ROUND_ENTERING &&
                             !i.nativeReloading;
        if (triggerEdge && !canFire && !i.nativeReloading && e == NO_EVENT)
            e = EV_DRY;
        o.event = e;
        o.open01 = closed_ ? 0 : open01_;
        o.closed = closed_;
        o.cocked = cocked_;
        o.heldAction = heldAction_;
        o.heldHammer = heldHammer_;
        o.blockFire = !canFire && !i.nativeReloading;
        o.round = round_;
        o.holdRound = round_ == ROUND_HELD || round_ == ROUND_ENTERING;
        o.roundEntering = round_ == ROUND_ENTERING;
        o.glowRound = o.holdRound;
        o.glowAction = i.hasAction && !heldAction_ && !o.holdRound &&
                       ((closed_ && i.clip <= 0 && i.canLoad) || (!closed_ && !i.canLoad));
        o.glowHammer = i.needsHammer && closed_ && !cocked_ && i.clip > 0 && !heldHammer_ && !o.holdRound;
        o.claimFree = heldAction_ || heldHammer_ || round_ != ROUND_NONE || (i.hasAction && i.nearAction) ||
                      (i.nearHammer && o.glowHammer) ||
                      (i.atTorso && i.canLoad && (!i.hasAction || !closed_));
        return o;
    }
};
}
