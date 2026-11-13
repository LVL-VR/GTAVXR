#pragma once
// build151 P3 (plan approval 2026-10-19 "APRUEBO +P1, P2, Y P3"; build149 wish: "las escopetas ... la corredera ... yo lo tenga que
// deslizar fisicamente al disparar"): a pump-action shotgun must be pumped by the free hand after every shot. Parts model:
//   READY (shell chambered) -> shot -> NEED_PUMP (spent shell in the chamber, the fore-end glows) -> the free hand grips the
//   fore-end (HELD; it rides the hand along the barrel) -> full stroke back = the shell flies out -> forward again = the next
 
// Fire needs READY with the fore-end forward and not held. A pump while READY ejects a live shell (no ammo change: native ammo is
 
// Self-review (Sol 151-4 partial finding, verified): letting go at the BACK of the stroke leaves the fore-end parked back (shown
// back, fire blocked); only grabbing it again and pushing it forward chambers. A grab of a parked fore-end starts at the back.
 
// the free hand takes ONE shell at the torso (it glows in the hand), brings its tip to the loading port under the receiver
// (aligned with the tube), pushes it in = exactly one shell into the gun (the glue's native clip transaction, from the reserve
// only, never beyond the clip size); repeat per shell. An EMPTY gun loaded this way still needs the pump to chamber (the fore-end
 
// build152 (plan "APRUEBO EL PLAN 152": shotguns complete): `semi` = a semi-automatic tube shotgun (Combat, Sweeper): a shot
// needs no pump; the charging part (lever/handle) springs back forward when let go; it must be charged only after the gun ran
// empty and shells were loaded (EMPTY -> shell in -> NEED_PUMP -> charge back + release -> READY).
 
#include <stdint.h>
namespace gx_pump
{
// Host supplies the measured native default.
double defaultStroke();
enum State
{
    READY,
    NEED_PUMP,
    EMPTY
};
enum Shell
{
    SHELL_NONE,
    SHELL_HELD,
    SHELL_ENTERING,
    SHELL_SEATING
};
enum Event
{
    NO_EVENT,
    EV_SHOT,
    EV_GRAB,
    EV_BACK,
    EV_FORWARD,
    EV_LET_GO,
    EV_DRY,
    EV_RELOADED,
    EV_CANCEL,
    EV_SHELL_GRAB,
    EV_SHELL_DROP,
    EV_SHELL_ALIGN,
    EV_SHELL_IN,
    EV_SHELL_SEATED,
    EVENT_COUNT
};
struct Input
{
    bool live;
    uint64_t weapon;
    int clip;
    bool nativeReloading, freeOwned;
    double freeGrip;
    bool nearPump;
    double pull, stroke;
    bool triggerPull;
    double now;
    // clip below its size and a reserve shell exists (native facts)
    bool canLoad;
    // free hand in the torso volume (take a shell)
    bool atTorso;
    // held shell tip at the loading port, aligned / still on the port axis; 0 mouth .. 1 in
    bool atPort, onPort;
    double portDepth;
    // the glue did the one-shell native transaction for the previous insert
    bool shellDirect;
    // build152: semi-automatic tube (no pump per shot; spring-return charging handle)
    bool semi;
    // build156: the free hand pose is valid (granted or not): grip release / press seen, grab edges allowed
    bool freeTracked;
    // build156: the free hand SUPPORTS the gun (trigger) at the fore-end: it pumps without grip and keeps SUPPORT
    bool supportHold;
    Input()
        : live(false), weapon(0), clip(0), nativeReloading(false), freeOwned(false), freeGrip(0),
          nearPump(false), pull(0), stroke(defaultStroke()), triggerPull(false), now(0), canLoad(false),
          atTorso(false), atPort(false), onPort(false), portDepth(0), shellDirect(false), semi(false),
          freeTracked(false), supportHold(false)
    {
    }
};
struct Output
{
    State state;
    bool held;
    Event event;
    bool blockFire, claimFree, capture, glowPump, shellLive, cancelled, graced;
    double pumpBack;
    // loadShell: do the one-shell native transaction NOW
    Shell shell;
    bool holdShell, shellEntering, glowShell, loadShell;
    // captureBack: this grab starts at the back (the glue offsets its pull reference by the stroke)
    bool captureBack, parkedBack;
    // build156: the fore-end is held by the supporting hand (the glue keeps SUPPORT and draws the hand on the fore-end)
    bool support;
    // build160 B (Codex reviewAH160 R8): the stroke reached the back this update, even when another event (a shot) took `event`
    bool backEdge;
    Output()
        : state(READY), held(false), event(NO_EVENT), blockFire(false), claimFree(false), capture(false),
          glowPump(false), shellLive(false), cancelled(false), graced(false), pumpBack(0), shell(SHELL_NONE),
          holdShell(false), shellEntering(false), glowShell(false), loadShell(false), captureBack(false),
          parkedBack(false), support(false), backEdge(false)
    {
    }
};
class Machine
{
    State state_;
    bool held_, back_, started_, gripDown_, gripNeutral_, triggerDown_, sawReload_;
    uint64_t weapon_;
    int lastClip_;
    double ownedAt_;
    // build156: the fore-end is held by the supporting hand (no RELOAD claim, follows the tracked hand)
    bool support_;
    Shell shell_;
    double shellAt_, entry_;

  public:
    Machine()
    {
        reset();
    }
    void reset()
    {
        state_ = READY;
        held_ = back_ = started_ = gripDown_ = gripNeutral_ = sawReload_ = support_ = false;
        triggerDown_ = true;
        weapon_ = 0;
        lastClip_ = 0;
        ownedAt_ = 0;
        shell_ = SHELL_NONE;
        shellAt_ = entry_ = 0;
    }
    Shell shell() const
    {
        return shell_;
    }
    static double SHELL_ENTRY_MAX()
    {
        return .4;
    }
    State state() const
    {
        return state_;
    }
    bool held() const
    {
        return held_;
    }
    static double HOLD_GRACE()
    {
        return .25;
    }
    Output update(const Input& i)
    {
        Output o;
        if (!i.live || !i.weapon || (weapon_ && i.weapon != weapon_))
        {
            o.cancelled = started_ && (held_ || back_ || state_ != READY || shell_ != SHELL_NONE);
            if (o.cancelled)
                o.event = EV_CANCEL;
            reset();
            return o;
        }
        if (!started_)
        {
            started_ = true;
            weapon_ = i.weapon;
            state_ = i.clip > 0 ? READY : EMPTY;
            lastClip_ = i.clip;
            triggerDown_ = i.triggerPull;
        }
        Event e = NO_EVENT;
         
        if (i.nativeReloading)
        {
            sawReload_ = true;
            held_ = false;
            back_ = false;
            support_ = false;
            shell_ = SHELL_NONE;
        }
        else if (sawReload_)
        {
            sawReload_ = false;
            state_ = i.clip > 0 ? READY : EMPTY;
            e = EV_RELOADED;
        }
        // a native shot
        if (i.clip < lastClip_ && !i.nativeReloading && state_ == READY)
        {
            state_ = i.semi ? (i.clip > 0 ? READY : EMPTY) : NEED_PUMP;
            e = EV_SHOT;
        }
        lastClip_ = i.clip;
        const bool triggerEdge = i.triggerPull && !triggerDown_;
        triggerDown_ = i.triggerPull;
        if (i.freeOwned)
            ownedAt_ = i.now;
        const bool graced = !i.freeOwned && gripDown_ &&
                            (held_ || shell_ == SHELL_HELD || shell_ == SHELL_ENTERING) &&
                            i.now - ownedAt_ <= HOLD_GRACE();
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
        bool gripEdge = freeHandAvailable && (gripNeutral_ || (i.nearPump)) && !gripDown_ &&
                        i.freeGrip >= .7 && (i.nearPump || (i.atTorso && i.canLoad));
        if (gripEdge)
        {
            gripDown_ = true;
            gripNeutral_ = false;
            if (!i.freeOwned)
                ownedAt_ = i.now;
        }
        const bool gripHeld = (i.freeOwned || graced) && gripDown_ && i.freeGrip >= .3;
        // ---- one shell at a time into the tube (the hand holding a shell cannot also hold the fore-end)
        bool load = false;
        if (!i.nativeReloading)
            switch (shell_)
            {
            case SHELL_NONE:
                if (!held_ && gripEdge && i.atTorso && i.canLoad)
                {
                    shell_ = SHELL_HELD;
                    shellAt_ = i.now;
                    if (e == NO_EVENT)
                        e = EV_SHELL_GRAB;
                    gripEdge = false;
                }
                break;
            case SHELL_HELD:
                if (!gripHeld)
                {
                    shell_ = SHELL_NONE;
                    if (e == NO_EVENT)
                        e = EV_SHELL_DROP;
                }
                else if (i.freeOwned && i.atPort && i.portDepth <= SHELL_ENTRY_MAX())
                {
                    shell_ = SHELL_ENTERING;
                    entry_ = i.portDepth;
                    if (e == NO_EVENT)
                        e = EV_SHELL_ALIGN;
                }
                break;
            case SHELL_ENTERING:
                // A complete tracked insertion also seats when grip is released in this sample.
                if (i.freeOwned && i.onPort && i.portDepth >= 1 && entry_ <= SHELL_ENTRY_MAX())
                {
                    if (i.canLoad)
                    {
                        shell_ = SHELL_SEATING;
                        shellAt_ = i.now;
                        load = true;
                        if (e == NO_EVENT)
                            e = EV_SHELL_IN;
                    }
                    // tube full meanwhile: it does not go in
                    else
                    {
                        shell_ = SHELL_NONE;
                        if (e == NO_EVENT)
                            e = EV_SHELL_DROP;
                    }
                }
                else if (!gripHeld)
                {
                    shell_ = SHELL_NONE;
                    if (e == NO_EVENT)
                        e = EV_SHELL_DROP;
                }
                // graced: hold, no edge
                else if (!i.freeOwned)
                    break;
                else if (!i.onPort)
                    shell_ = SHELL_HELD;
                break;
            case SHELL_SEATING:
                if (i.shellDirect || i.now - shellAt_ > 1.0)
                {
                    // in the tube, chamber still empty: pump it
                    shell_ = SHELL_NONE;
                    if (i.shellDirect)
                    {
                        if (state_ == EMPTY)
                            state_ = NEED_PUMP;
                        if (e == NO_EVENT)
                            e = EV_SHELL_SEATED;
                    }
                }
                break;
            }
        o.loadShell = load;
        const bool handBusy = shell_ == SHELL_HELD || shell_ == SHELL_ENTERING;
        if (!i.nativeReloading)
        {
            // build156: the supporting hand at the fore-end holds it (no grip, no RELOAD claim: SUPPORT keeps the hand and the aim)
            if (!held_ && !handBusy && i.supportHold && i.freeTracked)
            {
                held_ = true;
                support_ = true;
                o.capture = true;
                o.captureBack = back_;
                if (e == NO_EVENT)
                    e = EV_GRAB;
            }
            else if (!held_ && !handBusy && gripEdge && i.nearPump)
            {
                held_ = true;
                support_ = false;
                o.capture = true;
                o.captureBack = back_;
                if (e == NO_EVENT)
                    e = EV_GRAB;
            }
            else if (held_)
            {
                const double full = (i.stroke > .01 ? i.stroke : defaultStroke()) * .85;
                // build156: a supported fore-end moves with the tracked hand
                const bool moves = support_ ? i.freeTracked : i.freeOwned;
                // build156: support let go = let go
                const bool holds = support_ ? (i.supportHold && i.freeTracked) : gripHeld;
                if (holds && !back_ && moves && i.pull >= full)
                {
                    back_ = true;
                    o.backEdge = true;
                    o.shellLive = state_ == READY;
                    if (e == NO_EVENT)
                        e = EV_BACK;
                    state_ = state_ == EMPTY ? EMPTY : NEED_PUMP;
                }
                // forward: chamber the next shell (or empty)
                else if (holds && back_ && moves && i.pull < .01)
                {
                    if (!support_)
                        held_ = false;
                    back_ = false;
                    state_ = i.clip > 0 ? READY : EMPTY;
                    if (e == NO_EVENT)
                        e = EV_FORWARD;
                }
                // let go: a pump fore-end at the back stays parked back; a semi handle springs home
                else if (!holds)
                {
                    held_ = false;
                    support_ = false;
                    if (back_ && i.semi)
                    {
                        back_ = false;
                        state_ = i.clip > 0 ? READY : EMPTY;
                        if (e == NO_EVENT)
                            e = EV_FORWARD;
                    }
                    else if (e == NO_EVENT)
                        e = EV_LET_GO;
                }
            }
        }
        const bool canFire =
            state_ == READY && (!held_ || (support_ && i.pull < .01)) && !back_ && !i.nativeReloading;
        if (triggerEdge && !canFire && e == NO_EVENT)
            e = EV_DRY;
        o.state = state_;
        o.held = held_;
        o.event = e;
        o.blockFire = !canFire && !i.nativeReloading;
        // build156: a fore-end held by the supporting hand never claims RELOAD (SUPPORT outranks nothing here: RELOAD would win)
        o.claimFree = (held_ && !support_) || (!i.supportHold && i.nearPump && (state_ != READY || back_)) ||
                      shell_ != SHELL_NONE || (i.atTorso && i.canLoad);
        o.support = held_ && support_;
        if (o.support)
            o.claimFree = false;
        o.glowPump = state_ == NEED_PUMP && !held_ && !handBusy;
        o.shell = shell_;
        o.holdShell = handBusy;
        o.shellEntering = shell_ == SHELL_ENTERING;
        o.glowShell = handBusy;
        o.pumpBack = o.capture ? (o.captureBack ? i.stroke : 0)
                     : held_   ? (i.pull > 0 ? (i.pull > i.stroke ? i.stroke : i.pull) : 0)
                               : (back_ ? i.stroke : 0);
        o.parkedBack = back_ && !held_;
        return o;
    }
};
}
