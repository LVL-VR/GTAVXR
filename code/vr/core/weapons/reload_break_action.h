#pragma once
 
// weapons as live parts. Double barrel (2 chambers), flare gun, Marksman pistol (1 chamber, hammer cocked by hand) and the
// Perico pistol (rolling hammer: pulling the hammer fully back opens its chamber, pushing it home closes and cocks it).
//   CLOSED -> open (build159: with the latch released - gun hand grip + A - the barrels swing freely or the free hand swings
//   them down) -> spent cases eject when
//   fully open (live rounds stay) -> one round per push into an empty chamber mouth (taken at the torso; it glows) -> close
//   (latch released: a snap up, gravity or the hand) -> fire. The hammer (when the weapon needs it) is pulled back by the
//   free hand (or the gun hand's thumb button) before every shot.
 
 
#include <stdint.h>
// build159 F: the latch (grip + A) frees the hinge; replaces the 158 wrist flick
#include "reload_free_part.h"
namespace gx_break
{
enum Chamber
{
    EMPTY,
    LIVE,
    SPENT
};
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
    EV_EJECT,
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
static const int MAX_CHAMBERS = 2;
struct Input
{
    // weapon facts (spec)
    bool live;
    uint64_t weapon;
    int clip;
    int chambers;
    bool needsHammer, hammerOpens;
    bool nativeReloading, canLoad, freeOwned;
    double freeGrip;
    bool triggerPull;
    double now;
    // build156: the free hand pose is valid (granted or not)
    bool freeTracked;
    // free hand at the barrels (or the hammer for hammerOpens); its swing as 0 closed .. 1 open
    bool nearGrab;
    double grabOpen01;
    // gun-hand pitch rate (rad/s), + = muzzle up, - = muzzle down
    double flick;
    bool atTorso;
    // the held round vs the nearest EMPTY chamber mouth (0 mouth .. 1 in)
    int portChamber;
    bool atPort, onPort;
    double portDepth;
    // the glue did the one-round native transaction for the previous insert
    bool roundDirect;
    bool nearHammer;
    double hammerPull01;
    bool thumbCock;
    // build159 F: latch released (gun hand grip + A/X); the gun rate about the hinge (+ = opening sense), pseudo-gravity about it
    // (rad/s^2, + = opening) and the full swing (rad)
    bool latch;
    double gunRate, freeAccel, openRad;
    Input()
        : live(false), weapon(0), clip(0), chambers(1), needsHammer(false), hammerOpens(false),
          nativeReloading(false), canLoad(false), freeOwned(false), freeTracked(false), freeGrip(0),
          triggerPull(false), now(0), nearGrab(false), grabOpen01(0), flick(0), atTorso(false),
          portChamber(-1), atPort(false), onPort(false), portDepth(0), roundDirect(false), nearHammer(false),
          hammerPull01(0), thumbCock(false), latch(false), gunRate(0), freeAccel(0), openRad(1.0)
    {
    }
};
struct Output
{
    Event event;
    double open01;
    bool closed, cocked, heldGrab, heldHammer, blockFire, claimFree, capture, graced, cancelled;
    // ejected: how many spent cases flew out this update
    Chamber chamber[MAX_CHAMBERS];
    int ejected;
    Round round;
    int roundChamber;
    bool holdRound, roundEntering, loadRound;
    bool glowGrab, glowRound, glowHammer;
    double hammer01;
    Output()
        : event(NO_EVENT), open01(0), closed(true), cocked(false), heldGrab(false), heldHammer(false),
          blockFire(false), claimFree(false), capture(false), graced(false), cancelled(false), ejected(0),
          round(ROUND_NONE), roundChamber(-1), holdRound(false), roundEntering(false), loadRound(false),
          glowGrab(false), glowRound(false), glowHammer(false), hammer01(0)
    {
        chamber[0] = chamber[1] = EMPTY;
    }
};
class Machine
{
    bool started_, closed_, unlatched_, ejectedOpen_, cocked_, heldGrab_, heldHammer_, gripDown_,
        gripNeutral_, triggerDown_, sawReload_, thumbDown_;
    // build160 B (worn 159 log: flare gun open/close sounds alternating every 15-78 ms, 19 times): closing engages the latch, as on
    // a real break action / crane and in H3VR; it opens again only after A is released and pressed again (relatch_)
    bool relatch_;
    double open01_, ownedAt_, roundAt_, entry_;
    uint64_t weapon_;
    int lastClip_;
    Chamber ch_[MAX_CHAMBERS];
    Round round_;
    int roundChamber_;
    // build159 F: the released hinge
    gx_free::Hinge hinge_;

  public:
    Machine()
    {
        reset();
    }
    void reset()
    {
        relatch_ = false;
        started_ = false;
        closed_ = true;
        unlatched_ = ejectedOpen_ = cocked_ = heldGrab_ = heldHammer_ = gripDown_ = gripNeutral_ =
            sawReload_ = thumbDown_ = false;
        triggerDown_ = true;
        open01_ = 0;
        ownedAt_ = roundAt_ = entry_ = 0;
        weapon_ = 0;
        lastClip_ = 0;
        ch_[0] = ch_[1] = EMPTY;
        round_ = ROUND_NONE;
        roundChamber_ = -1;
        hinge_.reset(0);
    }
    static double HOLD_GRACE()
    {
        return .25;
    }
    static double ENTRY_MAX()
    {
        return .4;
    }
    bool heldHammer() const
    {
        return heldHammer_;
    }
    bool closed() const
    {
        return closed_;
    }
    Chamber chamber(int k) const
    {
        return k >= 0 && k < MAX_CHAMBERS ? ch_[k] : EMPTY;
    }
    Round round() const
    {
        return round_;
    }
    bool heldGrab() const
    {
        return heldGrab_;
    }
    double open01() const
    {
        return open01_;
    }
    Output update(const Input& i, double dt)
    {
        Output o;
        const int n = i.chambers < 1 ? 1 : (i.chambers > MAX_CHAMBERS ? MAX_CHAMBERS : i.chambers);
        if (!i.live || !i.weapon || (weapon_ && i.weapon != weapon_))
        {
            o.cancelled = started_ && (heldGrab_ || heldHammer_ || round_ != ROUND_NONE || !closed_);
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
            for (int k = 0; k < MAX_CHAMBERS; ++k)
                ch_[k] = k < n && k < i.clip ? LIVE : EMPTY;
            cocked_ = !i.needsHammer;
        }
        Event e = NO_EVENT;
         
        if (i.nativeReloading)
        {
            sawReload_ = true;
            heldGrab_ = heldHammer_ = false;
            round_ = ROUND_NONE;
        }
         
        else if (sawReload_)
        {
            sawReload_ = false;
            closed_ = true;
            unlatched_ = ejectedOpen_ = false;
            open01_ = 0;
            cocked_ = true;
            hinge_.reset(0);
            for (int k = 0; k < MAX_CHAMBERS; ++k)
                ch_[k] = k < n && k < i.clip ? LIVE : EMPTY;
            e = EV_RELOADED;
        }
        // a native shot: the first live chamber becomes a spent case
        if (i.clip < lastClip_ && !i.nativeReloading)
        {
            for (int k = 0; k < n; ++k)
                if (ch_[k] == LIVE)
                {
                    ch_[k] = SPENT;
                    break;
                }
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
        const bool busy = heldGrab_ || heldHammer_ || round_ == ROUND_HELD || round_ == ROUND_ENTERING;
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
        bool gripEdge = freeHandAvailable && (gripNeutral_ || (i.nearGrab || i.nearHammer)) && !gripDown_ &&
                        i.freeGrip >= .7 && (i.nearGrab || i.nearHammer || (i.atTorso && i.canLoad));
        if (gripEdge)
        {
            gripDown_ = true;
            gripNeutral_ = false;
            if (!i.freeOwned)
                ownedAt_ = i.now;
        }
        const bool gripHeld = (i.freeOwned || graced) && gripDown_ && i.freeGrip >= .3;
        if (!i.latch)
            relatch_ = false;
        bool load = false;
        // build154 class fix: the glue measures a held part relative to its capture, which it stores AFTER this update; on the grab
        // update itself those measurements still belong to the previous capture, so a part grabbed this update is not moved by them.
        bool fresh = false;
        if (!i.nativeReloading)
        {
            // ---- the action (barrels / hammer-opened chamber)
            if (!heldGrab_ && !heldHammer_ && round_ == ROUND_NONE && gripEdge && i.nearGrab)
            {
                heldGrab_ = true;
                fresh = true;
                o.capture = true;
                gripEdge = false;
                if (e == NO_EVENT)
                    e = EV_GRAB;
            }
            if (heldGrab_ && !fresh)
            {
                if (!gripHeld)
                {
                    heldGrab_ = false;
                    if (e == NO_EVENT)
                        e = EV_LET_GO;
                }
                // build159 F: the hand moves the barrels only while the latch is released (a rolling hammer is a hand part)
                else if (i.freeOwned && ((i.latch && !relatch_) || i.hammerOpens))
                {
                    const double a = i.grabOpen01 < 0 ? 0 : (i.grabOpen01 > 1 ? 1 : i.grabOpen01);
                    hinge_.reset(a * i.openRad);
                    if (closed_)
                    {
                        if (a >= .15)
                        {
                            closed_ = false;
                            unlatched_ = true;
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
                            unlatched_ = ejectedOpen_ = false;
                            open01_ = 0;
                            if (i.hammerOpens)
                                cocked_ = true;
                            hinge_.reset(0);
                            relatch_ = !i.hammerOpens;
                            // closed: the support hand just holds the gun again
                            heldGrab_ = false;
                            if (e == NO_EVENT)
                                e = EV_CLOSE;
                        }
                    }
                }
            }
            // build159 F: released and not held, the barrels swing freely (inertia, pseudo-gravity, friction); not released, they
            // stay where they are. Home with the latch engaging = closed. A rolling hammer is moved by hand only.
            else if (!heldGrab_ && !i.hammerOpens)
            {
                if (i.latch && !relatch_)
                {
                    hinge_.step(true, i.gunRate, i.freeAccel, i.openRad, dt);
                    const double a = hinge_.angle() / i.openRad;
                    if (closed_)
                    {
                        if (a >= .15)
                        {
                            closed_ = false;
                            unlatched_ = true;
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
                            unlatched_ = ejectedOpen_ = false;
                            open01_ = 0;
                            hinge_.reset(0);
                            relatch_ = true;
                            if (e == NO_EVENT)
                                e = EV_CLOSE;
                        }
                    }
                }
                else
                {
                    hinge_.step(false, 0, 0, i.openRad, dt);
                    hinge_.reset(closed_ ? 0 : open01_ * i.openRad);
                }
            }
            if (!closed_ && open01_ >= .9 && !ejectedOpen_)
            {
                ejectedOpen_ = true;
                int out = 0;
                for (int k = 0; k < n; ++k)
                    if (ch_[k] == SPENT)
                    {
                        ch_[k] = EMPTY;
                        ++out;
                    }
                o.ejected = out;
                if (out && e == NO_EVENT)
                    e = EV_EJECT;
            }
            // ---- the hammer (pulled back by the free hand, or the gun hand's thumb button)
            if (i.needsHammer && !i.hammerOpens && closed_ && !cocked_)
            {
                const bool thumbEdge = i.thumbCock && !thumbDown_;
                if (thumbEdge)
                {
                    cocked_ = true;
                    if (e == NO_EVENT)
                        e = EV_COCK;
                }
                else if (!heldGrab_ && !heldHammer_ && round_ == ROUND_NONE && gripEdge && i.nearHammer)
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
                {
                    heldHammer_ = false;
                }
                else if (i.freeOwned && i.hammerPull01 >= .9)
                {
                    cocked_ = true;
                    heldHammer_ = false;
                    if (e == NO_EVENT)
                        e = EV_COCK;
                }
            }
            // ---- one round into an empty chamber of the OPEN action
            int empty = 0;
            for (int k = 0; k < n; ++k)
                if (ch_[k] == EMPTY)
                    ++empty;
            const bool open = !closed_ && open01_ >= .8;
            switch (round_)
            {
            case ROUND_NONE:
                if (!heldGrab_ && !heldHammer_ && gripEdge && i.atTorso && i.canLoad && empty > 0)
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
                else if (i.freeOwned && open && i.atPort && i.portChamber >= 0 && i.portChamber < n &&
                         ch_[i.portChamber] == EMPTY && i.portDepth <= ENTRY_MAX())
                {
                    round_ = ROUND_ENTERING;
                    roundChamber_ = i.portChamber;
                    entry_ = i.portDepth;
                    if (e == NO_EVENT)
                        e = EV_ROUND_ALIGN;
                }
                break;
            case ROUND_ENTERING:
                // A complete tracked insertion also seats when grip is released in this sample.
                if (i.freeOwned && open && i.onPort && i.portChamber == roundChamber_ && i.portDepth >= 1 &&
                    entry_ <= ENTRY_MAX())
                {
                    if (i.canLoad && ch_[roundChamber_] == EMPTY)
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
                        roundChamber_ = -1;
                        if (e == NO_EVENT)
                            e = EV_ROUND_DROP;
                    }
                }
                else if (!gripHeld)
                {
                    round_ = ROUND_NONE;
                    roundChamber_ = -1;
                    if (e == NO_EVENT)
                        e = EV_ROUND_DROP;
                }
                // graced: hold, no edge
                else if (!i.freeOwned)
                    break;
                else if (!open || !i.onPort || i.portChamber != roundChamber_)
                {
                    round_ = ROUND_HELD;
                    roundChamber_ = -1;
                }
                break;
            case ROUND_SEATING:
                if (i.roundDirect || i.now - roundAt_ > 1.0)
                {
                    if (i.roundDirect && roundChamber_ >= 0 && roundChamber_ < n)
                    {
                        ch_[roundChamber_] = LIVE;
                        if (e == NO_EVENT)
                            e = EV_ROUND_SEATED;
                    }
                    round_ = ROUND_NONE;
                    roundChamber_ = -1;
                }
                break;
            }
        }
        thumbDown_ = i.thumbCock;
        o.loadRound = load;
        int live = 0;
        for (int k = 0; k < n; ++k)
            if (ch_[k] == LIVE)
                ++live;
        const bool canFire = closed_ && cocked_ && live > 0 && !heldGrab_ && !i.nativeReloading;
        if (triggerEdge && !canFire && !i.nativeReloading && e == NO_EVENT)
            e = EV_DRY;
        o.event = e;
        o.open01 = closed_ ? 0 : open01_;
        o.closed = closed_;
        o.cocked = cocked_;
        o.heldGrab = heldGrab_;
        o.heldHammer = heldHammer_;
        o.blockFire = !canFire && !i.nativeReloading;
        for (int k = 0; k < MAX_CHAMBERS; ++k)
            o.chamber[k] = ch_[k];
        o.round = round_;
        o.roundChamber = roundChamber_;
        o.holdRound = round_ == ROUND_HELD || round_ == ROUND_ENTERING;
        o.roundEntering = round_ == ROUND_ENTERING;
        int spent = 0, empty = 0;
        for (int k = 0; k < n; ++k)
        {
            if (ch_[k] == SPENT)
                ++spent;
            if (ch_[k] == EMPTY)
                ++empty;
        }
        const bool wantsOpen = closed_ && (spent > 0 || (empty > 0 && i.canLoad));
        o.glowGrab = !heldGrab_ && !o.holdRound && (wantsOpen || (!closed_ && (empty == 0 || !i.canLoad)));
        o.glowRound = o.holdRound;
        o.glowHammer = i.needsHammer && !i.hammerOpens && closed_ && !cocked_ && live > 0 && !heldHammer_;
        o.hammer01 = i.hammerOpens ? o.open01 : (cocked_ ? 1 : 0);
        o.claimFree = heldGrab_ || heldHammer_ || round_ != ROUND_NONE ||
                      (i.nearGrab && (wantsOpen || !closed_)) || (i.nearHammer && o.glowHammer) ||
                      (i.atTorso && i.canLoad && !closed_ && empty > 0);
        return o;
    }
};
}
