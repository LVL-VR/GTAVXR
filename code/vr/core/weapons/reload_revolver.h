#pragma once
 
// as live parts. Swing-out revolvers (Revolver, Revolver Mk II, Double Action) and the Navy (cap-and-ball: cylinder swap).
//   Swing-out: CLOSED -> open (build159: with the latch released - gun hand grip + A - the crane swings freely or the free hand
//   swings it out to the left) -> tilt
//   the muzzle up -> every case (spent AND live) falls out -> load: a speedloader from the torso (6 at once into an EMPTY
//   cylinder) or, gripping with the trigger also held, single rounds one per chamber -> close (latch released: a snap, gravity or the hand) ->
//   double action: each trigger pull turns the cylinder one chamber; an empty or spent chamber only clicks.
//   Navy: pull the wedge forward -> pull the cylinder out (its rounds return to the reserve) -> take a loaded cylinder from the
//   torso and push it onto the arbor -> push the wedge home -> single action: cock the hammer before every shot (free hand or
//   the gun hand's thumb button).
// Native ammo changes only through the glue's DoReload arithmetic (loadAll: up to the empty chambers; loadRound: one) and the
 
 
#include <stdint.h>
// build159 F: the latch (grip + A) frees the crane; replaces the 158 wrist flick
#include "reload_free_part.h"
namespace gx_revolver
{
// Host supplies the measured native default.
double defaultOpenAngle();
enum Chamber
{
    EMPTY,
    LIVE,
    SPENT
};
enum Item
{
    ITEM_NONE,
    LOADER_HELD,
    LOADER_ENTERING,
    LOADER_SEATING,
    LOADER_EMPTY,
    ROUND_HELD,
    ROUND_ENTERING,
    ROUND_SEATING,
    CYL_OLD_HELD,
    CYL_NEW_HELD,
    CYL_ENTERING,
    CYL_SEATING
};
enum Event
{
    NO_EVENT,
    EV_SHOT,
    EV_DRY,
    EV_OPEN,
    EV_CLOSE,
    EV_EJECT,
    EV_LOADER_GRAB,
    EV_ROUND_GRAB,
    EV_ALIGN,
    EV_IN,
    EV_SEATED,
    // build154: EV_SPIN
    EV_DROP,
    EV_COCK,
    EV_CYL_OUT,
    EV_CYL_GRAB,
    EV_RELOADED,
    EV_CANCEL,
    EV_GRAB,
    EV_SPIN,
    EVENT_COUNT
};
static const int CHAMBERS = 6;
struct Input
{
    // swap = Navy (cylinder swap), needsHammer = single action
    bool live;
    uint64_t weapon;
    int clip;
    bool swap;
    bool needsHammer;
    bool nativeReloading, canLoad, freeOwned;
    double freeGrip, freeTrigger;
    bool triggerPull;
    double now;
    // build156: the free hand pose is valid (granted or not)
    bool freeTracked;
    // free hand at the cylinder; its swing-out about the crane as 0 closed .. 1 open (swing-out)
    bool nearCylinder;
    double swing01;
    // Navy: free hand at the wedge; pulled forward 0 home .. 1 out
    bool nearWedge;
    double wedge01;
    // gun-hand roll rate (rad/s), + = the direction that swings the cylinder out
    double roll;
    // gun pitch: + = muzzle up
    double muzzleUpDeg;
    bool atTorso;
    // held loader/round/cylinder vs its seat (0 mouth .. 1 in); round: chamber index
    int portChamber;
    bool atPort, onPort;
    double portDepth;
    // glue result of the previous loadAll (rounds actually loaded) / loadRound
    int loaded;
    bool roundDirect;
    bool nearHammer;
    double hammerPull01;
    bool thumbCock;
    // build154: the held open cylinder turned by the hand since the grab (radians about the crane axis, swing removed)
    double spin;
    // build159 F: latch released (gun hand grip + A/X); the gun rate about the crane (+ = opening sense), pseudo-gravity about it
    // (rad/s^2, + = opening) and the full swing (rad)
    bool latch;
    double gunRate, freeAccel, openRad;
    Input()
        : live(false), weapon(0), clip(0), swap(false), needsHammer(false), nativeReloading(false),
          canLoad(false), freeOwned(false), freeTracked(false), freeGrip(0), freeTrigger(0),
          triggerPull(false), now(0), nearCylinder(false), swing01(0), nearWedge(false), wedge01(0), roll(0),
          muzzleUpDeg(0), atTorso(false), portChamber(-1), atPort(false), onPort(false), portDepth(0),
          loaded(-1), roundDirect(false), nearHammer(false), hammerPull01(0), thumbCock(false), spin(0),
          latch(false), gunRate(0), freeAccel(0), openRad(defaultOpenAngle())
    {
    }
};
struct Output
{
    Event event;
    double open01;
    bool closed, cylinderOut, cocked, heldPart, blockFire, claimFree, capture, graced, cancelled;
    Chamber chamber[CHAMBERS];
    int index;
    int ejectedLive, ejectedSpent;
    Item item;
    int itemChamber;
    bool holdItem, itemEntering, loadAll, loadRound, unloadAll, dropOldCylinder;
    // build153: hammer shown back (cocked)
    bool glowCylinder, glowWedge, glowItem, glowHammer;
    double hammer01;
    bool hammerBack;
    Output()
        : event(NO_EVENT), open01(0), closed(true), cylinderOut(false), cocked(false), heldPart(false),
          blockFire(false), claimFree(false), capture(false), graced(false), cancelled(false), index(0),
          ejectedLive(0), ejectedSpent(0), item(ITEM_NONE), itemChamber(-1), holdItem(false),
          itemEntering(false), loadAll(false), loadRound(false), unloadAll(false), dropOldCylinder(false),
          glowCylinder(false), glowWedge(false), glowItem(false), glowHammer(false), hammer01(0),
          hammerBack(false)
    {
        for (int k = 0; k < CHAMBERS; ++k)
            chamber[k] = EMPTY;
    }
};
class Machine
{
    bool started_, closed_, ejectedOpen_, cylOut_, cocked_, heldCyl_, heldWedge_, heldHammer_, gripDown_,
        gripNeutral_, triggerDown_, sawReload_, thumbDown_, openAnim_, hammerBack_;
    // build160 B (worn 159 log: flare gun open/close sounds alternating every 15-78 ms, 19 times): closing engages the latch, as on
    // a real break action / crane and in H3VR; it opens again only after A is released and pressed again (relatch_)
    bool relatch_;
    double open01_, ownedAt_, itemAt_, entry_;
    uint64_t weapon_;
    int lastClip_, idx_, itemChamber_;
    Chamber ch_[CHAMBERS];
    Item item_;
    // build154: chamber steps already applied for this grab; the shooter chose the chamber
    int spinSteps_;
    bool manualIndex_;
    // build159 F: the released crane
    gx_free::Hinge hinge_;
    void fillFromClip(int clip)
    {
        for (int k = 0; k < CHAMBERS; ++k)
            ch_[(idx_ + k) % CHAMBERS] = k < clip ? LIVE : EMPTY;
    }

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
        ejectedOpen_ = cylOut_ = cocked_ = heldCyl_ = heldWedge_ = heldHammer_ = gripDown_ = gripNeutral_ =
            sawReload_ = thumbDown_ = openAnim_ = hammerBack_ = false;
        triggerDown_ = true;
        open01_ = ownedAt_ = itemAt_ = entry_ = 0;
        weapon_ = 0;
        lastClip_ = 0;
        idx_ = 0;
        itemChamber_ = -1;
        item_ = ITEM_NONE;
        for (int k = 0; k < CHAMBERS; ++k)
            ch_[k] = EMPTY;
        spinSteps_ = 0;
        manualIndex_ = false;
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
    // muzzle-up degrees that drops the cases out of an open cylinder
    static double TILT()
    {
        return 45.0;
    }
    // build154: one chamber = 60 degrees of cylinder turn
    static double STEP()
    {
        return 1.0471976;
    }
    bool closed() const
    {
        return closed_;
    }
    int index() const
    {
        return idx_;
    }
    Chamber chamber(int k) const
    {
        return k >= 0 && k < CHAMBERS ? ch_[k] : EMPTY;
    }
    Item item() const
    {
        return item_;
    }
    bool cylinderOut() const
    {
        return cylOut_;
    }
    bool heldCylinder() const
    {
        return heldCyl_;
    }
    bool heldWedge() const
    {
        return heldWedge_;
    }
    bool heldHammer() const
    {
        return heldHammer_;
    }
    Output update(const Input& i, double dt)
    {
        Output o;
        if (!i.live || !i.weapon || (weapon_ && i.weapon != weapon_))
        {
            o.cancelled = started_ && (heldCyl_ || heldWedge_ || heldHammer_ || item_ != ITEM_NONE ||
                                       !closed_ || cylOut_);
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
            fillFromClip(i.clip);
            cocked_ = !i.needsHammer;
        }
        Event e = NO_EVENT;
        if (i.nativeReloading)
        {
            sawReload_ = true;
            heldCyl_ = heldWedge_ = heldHammer_ = false;
            item_ = ITEM_NONE;
            itemChamber_ = -1;
        }
         
        // reopened it with A held)
        else if (sawReload_)
        {
            sawReload_ = false;
            closed_ = true;
            cylOut_ = false;
            ejectedOpen_ = openAnim_ = false;
            open01_ = 0;
            cocked_ = true;
            hammerBack_ = i.needsHammer;
            fillFromClip(i.clip);
            e = EV_RELOADED;
            hinge_.reset(0);
        }
        const bool triggerEdge = i.triggerPull && !triggerDown_;
        triggerDown_ = i.triggerPull;
        // a native shot (only a closed revolver fires; a clip drop while open / cylinder out is our own unloadAll): the chamber under
        // the hammer is spent, the cylinder turns
        if (i.clip < lastClip_ && !i.nativeReloading && closed_ && !cylOut_)
        {
            ch_[idx_] = SPENT;
            idx_ = (idx_ + 1) % CHAMBERS;
            hammerBack_ = false;
            if (i.needsHammer)
                cocked_ = false;
            if (e == NO_EVENT)
                e = EV_SHOT;
        }
        else if (triggerEdge && !i.nativeReloading && closed_ && !cylOut_ && cocked_ && ch_[idx_] != LIVE &&
                 !heldCyl_ && !heldWedge_)
        {
            // hammer on an empty/spent chamber: click, turn
            idx_ = (idx_ + 1) % CHAMBERS;
            hammerBack_ = false;
            if (i.needsHammer)
                cocked_ = false;
            if (e == NO_EVENT)
                e = EV_DRY;
        }
        else if (triggerEdge && !i.nativeReloading && (!closed_ || cylOut_ || !cocked_) && e == NO_EVENT)
            e = EV_DRY;
        lastClip_ = i.clip;
        if (i.freeOwned)
            ownedAt_ = i.now;
        const bool busy = heldCyl_ || heldWedge_ || heldHammer_ || item_ == LOADER_HELD ||
                          item_ == LOADER_ENTERING || item_ == LOADER_EMPTY || item_ == ROUND_HELD ||
                          item_ == ROUND_ENTERING || item_ == CYL_OLD_HELD || item_ == CYL_NEW_HELD ||
                          item_ == CYL_ENTERING;
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
        bool gripEdge = freeHandAvailable &&
                        (gripNeutral_ || (i.nearCylinder || i.nearWedge || i.nearHammer)) && !gripDown_ &&
                        i.freeGrip >= .7 &&
                        (i.nearCylinder || i.nearWedge || i.nearHammer || (i.atTorso && i.canLoad));
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
        bool loadAll = false, loadRound = false, unloadAll = false;
        // build154 class fix: the glue measures a held part relative to its capture, which it stores AFTER this update; on the grab
        // update itself those measurements still belong to the previous capture, so a part grabbed this update is not moved by them.
        bool fresh = false;
        int live = 0, spent = 0, empty = 0;
        if (!i.nativeReloading)
        {
            const bool handFree = !heldCyl_ && !heldWedge_ && !heldHammer_ && item_ == ITEM_NONE;
            if (!i.swap)
            {
                // ---- swing-out crane: by hand (grab the cylinder) or free on its released latch
                if (handFree && gripEdge && i.nearCylinder)
                {
                    heldCyl_ = true;
                    fresh = true;
                    openAnim_ = false;
                    o.capture = true;
                    gripEdge = false;
                    spinSteps_ = 0;
                    if (e == NO_EVENT)
                        e = EV_GRAB;
                }
                if (heldCyl_ && !fresh)
                {
                    if (!gripHeld)
                        heldCyl_ = false;
                    // build159 F: the hand moves the crane only while the latch is released
                    else if (i.freeOwned && i.latch && !relatch_)
                    {
                        const double a = i.swing01 < 0 ? 0 : (i.swing01 > 1 ? 1 : i.swing01);
                        hinge_.reset(a * i.openRad);
                        if (closed_)
                        {
                            if (a >= .2)
                            {
                                closed_ = false;
                                open01_ = a;
                                manualIndex_ = false;
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
                                ejectedOpen_ = false;
                                heldCyl_ = false;
                                hinge_.reset(0);
                                relatch_ = true;
                                if (e == NO_EVENT)
                                    e = EV_CLOSE;
                            }
                        }
                        // build154 (user: "giro de tambor"): the fully swung-out cylinder turned by the hand, one click per chamber
                        if (heldCyl_ && !closed_ && open01_ >= .9)
                        {
                            const double s = i.spin / STEP();
                            const int target = (int)(s < 0 ? s - .5 : s + .5);
                            if (target != spinSteps_)
                            {
                                idx_ = ((idx_ + target - spinSteps_) % CHAMBERS + CHAMBERS) % CHAMBERS;
                                spinSteps_ = target;
                                manualIndex_ = true;
                                if (e == NO_EVENT)
                                    e = EV_SPIN;
                            }
                        }
                    }
                }
                // build159 F: released and not held by the hand, the crane swings freely (inertia, pseudo-gravity, friction); not
                // released, it stays where it is. Home with the latch engaging = closed.
                else if (!heldCyl_)
                {
                    if (i.latch && !relatch_)
                    {
                        hinge_.step(true, i.gunRate, i.freeAccel, i.openRad, dt);
                        const double a = hinge_.angle() / i.openRad;
                        if (closed_)
                        {
                            if (a >= .2)
                            {
                                closed_ = false;
                                open01_ = a;
                                manualIndex_ = false;
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
                                ejectedOpen_ = false;
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
                // ---- tilt the muzzle up with the cylinder open: everything falls out (live rounds return to the reserve)
                if (!closed_ && open01_ >= .9 && i.muzzleUpDeg >= TILT() && !ejectedOpen_)
                {
                    int l = 0, s = 0;
                    for (int k = 0; k < CHAMBERS; ++k)
                    {
                        if (ch_[k] == LIVE)
                            ++l;
                        else if (ch_[k] == SPENT)
                            ++s;
                        ch_[k] = EMPTY;
                    }
                    ejectedOpen_ = true;
                    o.ejectedLive = l;
                    o.ejectedSpent = s;
                    if (l + s > 0)
                    {
                        if (l > 0)
                            unloadAll = true;
                        if (e == NO_EVENT)
                            e = EV_EJECT;
                    }
                }
            }
            else
            {
                // ---- Navy: the wedge, then the cylinder itself
                if (handFree && gripEdge && i.nearWedge)
                {
                    heldWedge_ = true;
                    fresh = true;
                    o.capture = true;
                    gripEdge = false;
                    if (e == NO_EVENT)
                        e = EV_GRAB;
                }
                if (heldWedge_ && !fresh)
                {
                    if (!gripHeld)
                        heldWedge_ = false;
                    else if (i.freeOwned)
                    {
                        const double a = i.wedge01 < 0 ? 0 : (i.wedge01 > 1 ? 1 : i.wedge01);
                        if (closed_)
                        {
                            if (a >= .2)
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
                            if (a <= .05 && !cylOut_)
                            {
                                closed_ = true;
                                open01_ = 0;
                                heldWedge_ = false;
                                if (e == NO_EVENT)
                                    e = EV_CLOSE;
                            }
                        }
                    }
                }
                // pull the cylinder off the arbor
                if (!closed_ && open01_ >= .9 && !cylOut_ && handFree && gripEdge && i.nearCylinder)
                {
                    int l = 0, s = 0;
                    for (int k = 0; k < CHAMBERS; ++k)
                    {
                        if (ch_[k] == LIVE)
                            ++l;
                        else if (ch_[k] == SPENT)
                            ++s;
                        ch_[k] = EMPTY;
                    }
                    cylOut_ = true;
                    item_ = CYL_OLD_HELD;
                    itemAt_ = i.now;
                    gripEdge = false;
                    o.ejectedLive = l;
                    o.ejectedSpent = s;
                    if (l > 0)
                        unloadAll = true;
                    if (e == NO_EVENT)
                        e = EV_CYL_OUT;
                }
            }
            // ---- an open action lowers the hammer
            if (!closed_ || cylOut_)
            {
                hammerBack_ = false;
                if (i.needsHammer)
                    cocked_ = false;
            }
            // ---- the hammer: the Navy (single action) must be cocked; the swing-out revolvers MAY be (build153: the gun hand's
            //      grip + stick down = the thumb, single action shown back); the free hand still cocks a single-action hammer
            if (closed_ && !cylOut_ && !hammerBack_)
            {
                if (i.thumbCock && !thumbDown_)
                {
                    cocked_ = true;
                    hammerBack_ = true;
                    if (e == NO_EVENT)
                        e = EV_COCK;
                }
                else if (i.needsHammer && !heldCyl_ && !heldWedge_ && !heldHammer_ && item_ == ITEM_NONE &&
                         gripEdge && i.nearHammer)
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
                    hammerBack_ = true;
                    heldHammer_ = false;
                    if (e == NO_EVENT)
                        e = EV_COCK;
                }
            }
            // ---- what the free hand carries
            for (int k = 0; k < CHAMBERS; ++k)
            {
                if (ch_[k] == LIVE)
                    ++live;
                else if (ch_[k] == SPENT)
                    ++spent;
                else
                    ++empty;
            }
            const bool open = !closed_ && open01_ >= .9;
            switch (item_)
            {
            case ITEM_NONE:
                if (!heldCyl_ && !heldWedge_ && !heldHammer_ && gripEdge && i.atTorso && i.canLoad)
                {
                    if (i.swap)
                    {
                        if (cylOut_)
                        {
                            item_ = CYL_NEW_HELD;
                            itemAt_ = i.now;
                            gripEdge = false;
                            if (e == NO_EVENT)
                                e = EV_CYL_GRAB;
                        }
                    }
                    else if (open && spent == 0 && empty > 0)
                    {
                        if (i.freeTrigger >= .6)
                        {
                            item_ = ROUND_HELD;
                            if (e == NO_EVENT)
                                e = EV_ROUND_GRAB;
                        }
                        else if (empty == CHAMBERS)
                        {
                            item_ = LOADER_HELD;
                            if (e == NO_EVENT)
                                e = EV_LOADER_GRAB;
                        }
                        if (item_ != ITEM_NONE)
                        {
                            itemAt_ = i.now;
                            gripEdge = false;
                        }
                    }
                }
                break;
            case LOADER_HELD:
            case ROUND_HELD:
            case CYL_NEW_HELD:
            {
                const bool round = item_ == ROUND_HELD;
                if (!gripHeld)
                {
                    item_ = ITEM_NONE;
                    if (e == NO_EVENT)
                        e = EV_DROP;
                    break;
                }
                const bool seatOpen =
                    i.swap ? cylOut_
                           : (open && (round ? (i.portChamber >= 0 && i.portChamber < CHAMBERS &&
                                                ch_[i.portChamber] == EMPTY)
                                             : empty == CHAMBERS));
                if (i.freeOwned && seatOpen && i.atPort && i.portDepth <= ENTRY_MAX())
                {
                    item_ = round ? ROUND_ENTERING : (i.swap ? CYL_ENTERING : LOADER_ENTERING);
                    itemChamber_ = round ? i.portChamber : -1;
                    entry_ = i.portDepth;
                    if (e == NO_EVENT)
                        e = EV_ALIGN;
                }
                break;
            }
            case LOADER_ENTERING:
            case ROUND_ENTERING:
            case CYL_ENTERING:
            {
                const Item back = item_ == ROUND_ENTERING
                                      ? ROUND_HELD
                                      : (item_ == CYL_ENTERING ? CYL_NEW_HELD : LOADER_HELD);
                const bool completed = i.freeOwned && i.onPort && i.portDepth >= 1 && entry_ <= ENTRY_MAX();
                if (!gripHeld && !completed)
                {
                    item_ = ITEM_NONE;
                    itemChamber_ = -1;
                    if (e == NO_EVENT)
                        e = EV_DROP;
                    break;
                }
                // Grace never admits a new geometry edge.
                if (!i.freeOwned)
                    break;
                const bool stillOpen = i.swap ? cylOut_ : open;
                if (!stillOpen || !i.onPort || (item_ == ROUND_ENTERING && i.portChamber != itemChamber_))
                {
                    item_ = back;
                    itemChamber_ = -1;
                    break;
                }
                if (i.portDepth >= 1 && entry_ <= ENTRY_MAX())
                {
                    if (!i.canLoad)
                    {
                        item_ = ITEM_NONE;
                        itemChamber_ = -1;
                        if (e == NO_EVENT)
                            e = EV_DROP;
                        break;
                    }
                    if (item_ == ROUND_ENTERING)
                    {
                        item_ = ROUND_SEATING;
                        loadRound = true;
                    }
                    else
                    {
                        item_ = item_ == CYL_ENTERING ? CYL_SEATING : LOADER_SEATING;
                        loadAll = true;
                    }
                    itemAt_ = i.now;
                    if (e == NO_EVENT)
                        e = EV_IN;
                }
                break;
            }
            case LOADER_SEATING:
            case CYL_SEATING:
                if (i.loaded >= 0 || i.now - itemAt_ > 1.0)
                {
                    int n = i.loaded > 0 ? i.loaded : 0;
                    for (int k = 0; k < CHAMBERS && n > 0; ++k)
                    {
                        int c = (idx_ + k) % CHAMBERS;
                        if (ch_[c] == EMPTY)
                        {
                            ch_[c] = LIVE;
                            --n;
                        }
                    }
                    if (item_ == CYL_SEATING)
                    {
                        cylOut_ = false;
                        item_ = ITEM_NONE;
                    }
                    // the emptied speedloader stays in the hand until let go
                    else
                        item_ = LOADER_EMPTY;
                    if (i.loaded >= 0 && e == NO_EVENT)
                        e = EV_SEATED;
                }
                break;
            case ROUND_SEATING:
                if (i.roundDirect || i.now - itemAt_ > 1.0)
                {
                    if (i.roundDirect && itemChamber_ >= 0)
                        ch_[itemChamber_] = LIVE;
                    item_ = ITEM_NONE;
                    itemChamber_ = -1;
                    if (i.roundDirect && e == NO_EVENT)
                        e = EV_SEATED;
                }
                break;
            case LOADER_EMPTY:
            case CYL_OLD_HELD:
                if (!gripHeld)
                {
                    if (item_ == CYL_OLD_HELD)
                        o.dropOldCylinder = true;
                    item_ = ITEM_NONE;
                    if (e == NO_EVENT)
                        e = EV_DROP;
                }
                break;
            }
            // ---- a revolver closed with the hammer on an empty chamber is turned to the first live one (the shooter indexes it)
            // build154: unless the shooter turned the cylinder by hand (then the chamber left on top fires first)
            if (e == EV_CLOSE && !manualIndex_ && ch_[idx_] != LIVE)
                for (int k = 1; k < CHAMBERS; ++k)
                {
                    int c = (idx_ + k) % CHAMBERS;
                    if (ch_[c] == LIVE)
                    {
                        idx_ = c;
                        break;
                    }
                }
        }
        thumbDown_ = i.thumbCock;
        live = spent = empty = 0;
        for (int k = 0; k < CHAMBERS; ++k)
        {
            if (ch_[k] == LIVE)
                ++live;
            else if (ch_[k] == SPENT)
                ++spent;
            else
                ++empty;
        }
        o.loadAll = loadAll;
        o.loadRound = loadRound;
        o.unloadAll = unloadAll;
        const bool canFire = closed_ && !cylOut_ && cocked_ && ch_[idx_] == LIVE && !heldCyl_ &&
                             !heldWedge_ && !i.nativeReloading;
        o.event = e;
        o.open01 = closed_ ? 0 : open01_;
        o.closed = closed_;
        o.cylinderOut = cylOut_;
        o.cocked = cocked_;
        o.heldPart = heldCyl_ || heldWedge_ || heldHammer_;
        o.blockFire = !canFire && !i.nativeReloading;
        for (int k = 0; k < CHAMBERS; ++k)
            o.chamber[k] = ch_[k];
        o.index = idx_;
        o.item = item_;
        o.itemChamber = itemChamber_;
        o.holdItem =
            item_ != ITEM_NONE && item_ != LOADER_SEATING && item_ != ROUND_SEATING && item_ != CYL_SEATING;
        o.itemEntering = item_ == LOADER_ENTERING || item_ == ROUND_ENTERING || item_ == CYL_ENTERING;
        const bool wantsOpen = closed_ && (spent > 0 || (empty > 0 && i.canLoad));
        o.glowCylinder = !i.swap && !heldCyl_ && !o.holdItem &&
                         (wantsOpen || (!closed_ && (spent > 0 || live + spent == CHAMBERS || !i.canLoad)));
        o.glowWedge =
            i.swap && !heldWedge_ && !o.holdItem && ((wantsOpen) || (!closed_ && !cylOut_ && live > 0));
        o.glowItem = o.holdItem && item_ != LOADER_EMPTY && item_ != CYL_OLD_HELD;
        o.glowHammer = i.needsHammer && closed_ && !cylOut_ && !cocked_ && live > 0 && !heldHammer_;
        o.hammer01 = hammerBack_ ? 1 : 0;
        o.hammerBack = hammerBack_;
        o.claimFree = heldCyl_ || heldWedge_ || heldHammer_ || item_ != ITEM_NONE ||
                      (i.nearCylinder && (!i.swap ? (wantsOpen || !closed_) : (!closed_ && !cylOut_))) ||
                      (i.nearWedge && i.swap && (wantsOpen || !closed_)) || (i.nearHammer && o.glowHammer) ||
                      (i.atTorso && i.canLoad && (i.swap ? cylOut_ : (!closed_ && empty > 0)));
        return o;
    }
};
}
