#pragma once
 
// muevan de forma realista. Y cada arma tienes que pasar por un proceso de identificar sus partes, de identificar como se
// recarga ... tiene que tener bien aplicado y adaptado su recarga completa, de principio a fin, de forma realista"; "si yo pongo
// mi mano con la palma mirando hacia arriba, el cargador deberia estar vertical en mi mano"; "VAMOS A OLVIDAR TANTO EL CARGADOR
// FANTASMA COMO LA ESFERA EN EL CARGADOR Y NOS ENFOCAREMOS EN LAS PARTES GLOWING"; plan approval "APRUEBO +P1, P2, Y P3").
// Replaces the 149/150 reload-only phase machine with a PARTS model of a magazine pistol that is live at all times:
//   magazine: IN (in the well) / OUT (ejected, none) / HELD (new or caught magazine in the free hand) / ENTERING (on the well
//             axis, depth follows the hand) / SEATING (native ammo transaction in flight) / QUICK (native Quick reload);
//   slide:    HOME / HELD (free hand pulling it; follows the hand along the barrel) / LOCKED (slide stop, locked back);
 
// Rules (real pistol): fire needs a chambered round with the slide home; a shot with the magazine out empties the chamber; the
// last round fired with a magazine in locks the slide back; pulling the slide fully back ejects a chambered round and, when it
// goes forward with a loaded magazine in, chambers the next one (none in -> it closes on an empty chamber; empty magazine in ->
// it locks back); a short reload press is the slide release while locked with a loaded magazine in, otherwise the magazine
 
// holding a magazine. Insertion needs entry at the well mouth with the magazine's axis and front aligned (Sol 150-3 F06) and a
// directed stroke to full depth. While the free hand is graced (brief grant/tracking loss, grip still held) the held part is
 
 
// chamber; the handle must be lifted, pulled back (case / round out), pushed forward (feeds from a loaded magazine) and lowered
// before the next shot. FEED COVER (MG, Combat MG): the box comes out / goes in only with the cover open; the gun fires only with
 
 
#include <stdint.h>
namespace gx_pistol
{
enum Mag
{
    MAG_IN,
    MAG_OUT,
    MAG_HELD,
    MAG_ENTERING,
    MAG_SEATING,
    MAG_QUICK
};
enum Slide
{
    SLIDE_HOME,
    SLIDE_HELD,
    SLIDE_LOCKED
};
enum Event
{
    NO_EVENT,
    EV_EJECT,
    EV_GRAB,
    EV_CATCH,
    EV_DROP,
    EV_ALIGN,
    EV_INSERT,
    EV_SEATED,
    EV_SLIDE_GRAB,
    EV_SLIDE_BACK,
    EV_SLIDE_HOME,
    EV_SLIDE_LOCK,
    EV_SLIDE_RELEASE,
    EV_CHAMBER,
    EV_ROUND_OUT,
    EV_QUICK,
    EV_CANCEL,
    EV_EMPTY,
    EV_DRY,
    // build154: bolt + feed cover
    EV_BOLT_UP,
    EV_BOLT_DOWN,
    EV_COVER_OPEN,
    EV_COVER_CLOSE,
    EVENT_COUNT
};
struct Input
{
    // realistic pistol mode, eligible actor/weapon/context, gun hand tracked
    bool live;
     
    uint64_t weapon;
    // the player's reload control (level)
    bool reloadInput;
    // gun-hand fire control pressed this update (edge measured here) -> dry click when it cannot fire
    bool triggerPull;
    // native rounds in the clip now
    int clip;
     
    bool canReload;
    // native reload permission now (TaskGun CanReload/blocks, no legacy reload task) (Sol F09)
    bool permit;
     
    bool nativeReloading;
     
    bool seatedDirect;
    // the free hand is tracked and granted to RELOAD by the arbiter
    bool freeOwned;
    // build156: the free hand pose is valid (granted or not): grip release / press seen, grab edges allowed
    bool freeTracked;
    // free hand grip axis 0..1
    double freeGrip;
    // free hand inside the torso volume (take a new magazine)
    bool atTorso;
    // free hand at the falling ejected magazine inside the catch window (P2)
    bool canCatch;
    // held magazine top at the well mouth, axis and front aligned (entry gate)
    bool atMouth;
    // held magazine still on the well axis (stay gate while ENTERING)
    bool onAxis;
    // 0 = magazine top at the well mouth .. 1 = fully seated (valid while onAxis)
    double depth;
    // free hand at the slide grip point
    bool nearSlide;
    // free-hand travel back along the barrel since the slide capture, metres (>=0)
    double pull;
    // full slide stroke for this pistol, metres (from the parts atlas)
    double stroke;
    // seconds
    double now;
    // build153: the slide/bolt locks back on an empty magazine (false: AK / MP5 / Uzi / TEC-9 close empty)
    bool holdOpen;
    // build154: manual bolt action (every shot must be cycled by hand)
    bool bolt;
    // build154: bolt handle rotation since the capture, 0 .. 1 = fully up (signed; the machine adds the base)
    double lift01;
    // build154: a feed cover (MG): box swaps need it open, fire needs it closed
    bool coverPresent;
    // build154: free hand at the cover's grab point
    bool nearCover;
    // build154: cover angle driven by the held hand, 0 closed .. 1 open (the glue adds the capture base)
    double cover01;
    Input()
        : live(false), weapon(0), reloadInput(false), triggerPull(false), clip(0), canReload(false),
          permit(false), nativeReloading(false), seatedDirect(false), freeOwned(false), freeTracked(false),
          freeGrip(0), atTorso(false), canCatch(false), atMouth(false), onAxis(false), depth(0),
          nearSlide(false), pull(0), stroke(.05), now(0), holdOpen(true), bolt(false), lift01(0),
          coverPresent(false), nearCover(false), cover01(0)
    {
    }
};
enum DropWhy
{
    DROP_NONE,
    DROP_GRIP,
    DROP_LOST
};
struct Output
{
    // event2: a second edge in the same update (slide home + chamber)
    Mag mag;
    Slide slide;
    bool chambered;
    Event event, event2;
    bool hideMagazine, blockFire, claimFree, startNative, quick, captureSlide, cancelled, graced;
    // visuals: magazine proxy in the hand / on the axis; slide travel shown (metres)
    bool holdProxy, entering;
    double slideBack;
    // MW2-style glowing part: the part the player must move now
    bool glowMagazine, glowSlide;
    // build154: bolt handle 0..1 shown, cover 0..1 shown
    double lift, cover;
    bool glowCover, captureCover, boltUp;
    DropWhy dropWhy;
    Output()
        : mag(MAG_IN), slide(SLIDE_HOME), chambered(true), event(NO_EVENT), event2(NO_EVENT),
          hideMagazine(false), blockFire(false), claimFree(false), startNative(false), quick(false),
          captureSlide(false), cancelled(false), graced(false), holdProxy(false), entering(false),
          slideBack(0), glowMagazine(false), glowSlide(false), lift(0), cover(0), glowCover(false),
          captureCover(false), boltUp(false), dropWhy(DROP_NONE)
    {
    }
};
class Machine
{
    Mag mag_;
    Slide slide_;
    bool chambered_, started_;
    uint64_t weapon_;
    bool reloadDown_, longDone_, gripDown_, gripNeutral_, backReached_, fromLocked_, triggerDown_, sawNative_;
    int lastClip_;
    double since_, pressAt_, ownedAt_, entryDepth_;
    // build154: bolt + feed cover
    bool up_, boltBack_, spent_, grabUp_, grabBack_, coverHeld_;
    double cover_;
    void mag(Mag m, double now)
    {
        mag_ = m;
        since_ = now;
        sawNative_ = false;
    }

  public:
    Machine()
    {
        reset();
    }
    void reset()
    {
        mag_ = MAG_IN;
        slide_ = SLIDE_HOME;
        chambered_ = true;
        started_ = false;
        weapon_ = 0;
        reloadDown_ = true;
        longDone_ = true;
        gripDown_ = false;
        gripNeutral_ = false;
        backReached_ = false;
        fromLocked_ = false;
        triggerDown_ = true;
        sawNative_ = false;
        lastClip_ = 0;
        since_ = pressAt_ = ownedAt_ = entryDepth_ = 0;
        up_ = boltBack_ = spent_ = grabUp_ = grabBack_ = coverHeld_ = false;
        cover_ = 0;
    }
    Mag magazine() const
    {
        return mag_;
    }
    Slide slide() const
    {
        return slide_;
    }
    bool chambered() const
    {
        return chambered_;
    }
    bool boltUp() const
    {
        return up_;
    }
    bool boltBack() const
    {
        return boltBack_;
    }
    double cover() const
    {
        return cover_;
    }
    bool coverHeld() const
    {
        return coverHeld_;
    }
    static double SEAT_TIMEOUT()
    {
        return 1.0;
    }
    static double LONG_PRESS()
    {
        return .6;
    }
    static double HOLD_GRACE()
    {
        return .25;
    }
    // a locked slide only needs a short tug back to release (then let it go)
    static double LOCKED_TUG()
    {
        return .012;
    }
    // insertion must start in the first 35 % of the stroke (Sol F06)
    static double ENTRY_MAX()
    {
        return .35;
    }
    Output update(const Input& i)
    {
        Output o;
        const bool changed = weapon_ && i.weapon != weapon_;
        if (!i.live || !i.weapon || changed)
        {
            // Exit: nothing native changed before a seat, so the magazine simply shows again; the next entry re-reads the gun.
            o.cancelled = started_ && (mag_ != MAG_IN || slide_ == SLIDE_HELD);
            if (o.cancelled)
                o.event = EV_CANCEL;
            const bool held = reloadDown_ || i.reloadInput;
            reset();
            reloadDown_ = held;
            o.mag = MAG_IN;
            o.slide = SLIDE_HOME;
            o.chambered = true;
            return o;
        }
        // first live update for this weapon: read the gun as it is
        if (!started_)
        {
            started_ = true;
            weapon_ = i.weapon;
            chambered_ = i.clip > 0;
            slide_ = i.clip > 0 || !i.holdOpen || i.bolt ? SLIDE_HOME : SLIDE_LOCKED;
            lastClip_ = i.clip;
            triggerDown_ = i.triggerPull;
        }
        // A native shot happened since the last update (the clip went down while we allowed fire).
        if (i.clip < lastClip_ && mag_ != MAG_SEATING && mag_ != MAG_QUICK)
        {
            // build154: the case stays until the bolt is cycled
            if (i.bolt)
            {
                chambered_ = false;
                spent_ = true;
                if (i.clip <= 0)
                    o.event = EV_EMPTY;
            }
            else if (mag_ == MAG_IN && i.clip > 0)
                chambered_ = true;
            else
            {
                chambered_ = false;
                if (mag_ == MAG_IN && slide_ == SLIDE_HOME)
                {
                    if (i.holdOpen)
                        slide_ = SLIDE_LOCKED;
                    o.event = EV_EMPTY;
                }
            }
        }
        lastClip_ = i.clip;
        // Reload control: short press acts on RELEASE (<0.6 s); a long press acts once while held (>=0.6 s).
        bool shortPress = false, longPress = false;
        if (i.reloadInput && !reloadDown_)
        {
            pressAt_ = i.now;
            longDone_ = false;
        }
        if (i.reloadInput && !longDone_ && i.now - pressAt_ >= LONG_PRESS())
        {
            longPress = true;
            longDone_ = true;
        }
        if (!i.reloadInput && reloadDown_ && !longDone_)
        {
            shortPress = true;
            longDone_ = true;
        }
        reloadDown_ = i.reloadInput;
        const bool triggerEdge = i.triggerPull && !triggerDown_;
        triggerDown_ = i.triggerPull;
        // Free-hand grip with a short grace that keeps a held part but admits no geometry edge (Sol F08).
        if (i.freeOwned)
            ownedAt_ = i.now;
        const bool holding = mag_ == MAG_HELD || mag_ == MAG_ENTERING || slide_ == SLIDE_HELD || coverHeld_;
        const bool graced = !i.freeOwned && gripDown_ && holding && i.now - ownedAt_ <= HOLD_GRACE();
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
        const bool gripEdge = freeHandAvailable && (gripNeutral_ || (i.nearSlide || i.nearCover)) &&
                              !gripDown_ && i.freeGrip >= .7 &&
                              (i.nearSlide || i.nearCover || (mag_ == MAG_OUT && (i.atTorso || i.canCatch)));
        if (gripEdge)
        {
            gripDown_ = true;
            gripNeutral_ = false;
            if (!i.freeOwned)
                ownedAt_ = i.now;
        }
        const bool gripHeld = (i.freeOwned || graced) && gripDown_ && i.freeGrip >= .3;
        // geometry edges only with a currently owned, tracked free hand
        const bool geo = i.freeOwned;
        // build156: a grab edge uses the tracked pose (the grant follows next update)
        const bool grabGeo = geo || i.freeTracked;
        const DropWhy why = i.freeOwned || graced ? DROP_GRIP : DROP_LOST;
        Event e = o.event, e2 = NO_EVENT;
         
        if (longPress && mag_ != MAG_SEATING && mag_ != MAG_QUICK)
        {
            if (i.canReload && i.permit)
            {
                e = EV_QUICK;
                slide_ = SLIDE_HOME;
                mag(MAG_QUICK, i.now);
                up_ = boltBack_ = spent_ = coverHeld_ = false;
                cover_ = 0;
            }
        }
        // ---------------------------------------------------------------- the magazine
        else
            switch (mag_)
            {
            case MAG_IN:
                if (shortPress)
                {
                    if (slide_ == SLIDE_LOCKED && i.clip > 0)
                    {
                        slide_ = SLIDE_HOME;
                        chambered_ = true;
                        e = EV_SLIDE_RELEASE;
                        e2 = EV_CHAMBER;
                    }
                    // build154: box out only with the cover open
                    else if (slide_ != SLIDE_HELD && (!i.coverPresent || cover_ >= .8))
                    {
                        e = EV_EJECT;
                        mag(MAG_OUT, i.now);
                    }
                }
                break;
            case MAG_OUT:
                if (gripEdge && slide_ != SLIDE_HELD && i.canCatch)
                {
                    e = EV_CATCH;
                    mag(MAG_HELD, i.now);
                }
                else if (gripEdge && slide_ != SLIDE_HELD && i.atTorso)
                {
                    e = EV_GRAB;
                    mag(MAG_HELD, i.now);
                }
                // nothing to feed: closes empty
                else if (shortPress && slide_ == SLIDE_LOCKED)
                {
                    slide_ = SLIDE_HOME;
                    e = EV_SLIDE_RELEASE;
                }
                break;
            case MAG_HELD:
                if (!gripHeld)
                {
                    e = EV_DROP;
                    o.dropWhy = why;
                    mag(MAG_OUT, i.now);
                }
                else if (geo && i.atMouth && i.depth <= ENTRY_MAX() && (!i.coverPresent || cover_ >= .8))
                {
                    e = EV_ALIGN;
                    entryDepth_ = i.depth < 0 ? 0 : i.depth;
                    mag(MAG_ENTERING, i.now);
                }
                break;
            case MAG_ENTERING:
                if (geo && i.onAxis && i.depth >= 1 && entryDepth_ <= ENTRY_MAX())
                {
                    e = EV_INSERT;
                    if (i.canReload && i.permit)
                        mag(MAG_SEATING, i.now);
                    else
                    {
                        mag(MAG_IN, i.now);
                        e2 = EV_SEATED;
                    }
                }
                else if (!gripHeld)
                {
                    e = EV_DROP;
                    o.dropWhy = why;
                    mag(MAG_OUT, i.now);
                }
                else if (!geo)
                    break;
                else if (!i.onAxis)
                {
                    mag(MAG_HELD, i.now);
                }
                break;
            // the direct seat (lone-right) or the native task path (extended layout) finishes it
            case MAG_SEATING:
                if (i.seatedDirect)
                {
                    mag(MAG_IN, i.now);
                    e2 = EV_SEATED;
                }
                else if (i.nativeReloading)
                    sawNative_ = true;
                else if (sawNative_)
                {
                    sawNative_ = false;
                    mag(MAG_IN, i.now);
                    e2 = EV_SEATED;
                }
                // refused: no ammo change
                else if (i.now - since_ > SEAT_TIMEOUT())
                {
                    o.cancelled = true;
                    e = EV_CANCEL;
                    mag(MAG_IN, i.now);
                }
                break;
             
            case MAG_QUICK:
                if (i.nativeReloading)
                    sawNative_ = true;
                else if (sawNative_ || i.now - since_ > SEAT_TIMEOUT())
                {
                    const bool done = sawNative_;
                    sawNative_ = false;
                    mag(MAG_IN, i.now);
                    chambered_ = i.clip > 0;
                    slide_ = i.clip > 0 || !i.holdOpen || i.bolt ? SLIDE_HOME : SLIDE_LOCKED;
                    up_ = boltBack_ = spent_ = false;
                    if (done)
                        e2 = EV_SEATED;
                    else
                    {
                        o.cancelled = true;
                        e = EV_CANCEL;
                    }
                }
                break;
            }
        // ---------------------------------------------------------------- the slide (any time the free hand is not holding a magazine)
        const bool handBusy = mag_ == MAG_HELD || mag_ == MAG_ENTERING;
        bool coverTook = false;
        // build154: the feed cover (grab it, swing it with the hand; let go: open past half stays open, else it shuts)
        if (i.coverPresent)
        {
            if (!coverHeld_ && slide_ != SLIDE_HELD && !handBusy && mag_ != MAG_SEATING &&
                mag_ != MAG_QUICK && grabGeo && gripEdge && i.nearCover && e != EV_CATCH && e != EV_GRAB)
            {
                coverHeld_ = true;
                coverTook = true;
                o.captureCover = true;
            }
            else if (coverHeld_ && !coverTook)
            {
                if (!gripHeld)
                {
                    coverHeld_ = false;
                    const bool open = cover_ >= .5;
                    cover_ = open ? 1 : 0;
                    if (e == NO_EVENT)
                        e = open ? EV_COVER_OPEN : EV_COVER_CLOSE;
                }
                else if (geo)
                    cover_ = i.cover01 < 0 ? 0 : (i.cover01 > 1 ? 1 : i.cover01);
            }
        }
        if (slide_ != SLIDE_HELD && !handBusy && !coverHeld_ && !coverTook && mag_ != MAG_SEATING &&
            mag_ != MAG_QUICK && grabGeo && gripEdge && i.nearSlide && e != EV_CATCH && e != EV_GRAB)
        {
            fromLocked_ = slide_ == SLIDE_LOCKED;
            backReached_ = false;
            slide_ = SLIDE_HELD;
            o.captureSlide = true;
            if (e == NO_EVENT)
                e = EV_SLIDE_GRAB;
            grabUp_ = up_;
            grabBack_ = boltBack_;
        }
        // build154: lift, back (case / round out), forward (feeds), down (locked)
        else if (slide_ == SLIDE_HELD && i.bolt)
        {
            const double lift = (grabUp_ ? 1. : 0.) + i.lift01, pull = (grabBack_ ? i.stroke : 0.) + i.pull,
                         full = (i.stroke > .01 ? i.stroke : .06) * .85;
            if (!up_ && geo && lift >= .8)
            {
                up_ = true;
                if (e == NO_EVENT)
                    e = EV_BOLT_UP;
            }
            if (up_ && geo)
            {
                if (!boltBack_ && pull >= full)
                {
                    boltBack_ = true;
                    if (e == NO_EVENT)
                        e = EV_SLIDE_BACK;
                    if (chambered_ || spent_)
                    {
                        if (chambered_)
                            e2 = EV_ROUND_OUT;
                        chambered_ = false;
                        spent_ = false;
                    }
                }
                else if (boltBack_ && pull < .008)
                {
                    boltBack_ = false;
                    if (mag_ == MAG_IN && i.clip > 0)
                    {
                        chambered_ = true;
                        if (e == NO_EVENT)
                            e = EV_SLIDE_HOME;
                        e2 = EV_CHAMBER;
                    }
                    else if (e == NO_EVENT)
                        e = EV_SLIDE_HOME;
                }
                else if (!boltBack_ && pull < .008 && lift <= .2)
                {
                    up_ = false;
                    if (e == NO_EVENT)
                        e = EV_BOLT_DOWN;
                }
            }
            // let go: the bolt stays where it is (up / back)
            if (!gripHeld)
                slide_ = SLIDE_HOME;
            o.lift = lift < 0 ? 0 : (lift > 1 ? 1 : lift);
            if (up_)
            {
                const double shown = pull < 0 ? 0 : (pull > i.stroke ? i.stroke : pull);
                o.slideBack = shown;
            }
        }
        else if (slide_ == SLIDE_HELD)
        {
            const double full = fromLocked_ ? LOCKED_TUG() : (i.stroke > .01 ? i.stroke * .85 : .035);
            if (!backReached_ && geo && i.pull >= full)
            {
                backReached_ = true;
                if (e == NO_EVENT)
                    e = EV_SLIDE_BACK;
                // a chambered round flies out (no ammo change)
                if (chambered_ && !fromLocked_)
                {
                    chambered_ = false;
                    e2 = EV_ROUND_OUT;
                }
            }
            else if (!gripHeld || (backReached_ && geo && i.pull < .008))
            {
                // Let go: the slide springs forward. After a full stroke it feeds from a loaded magazine, or locks on an empty one.
                if (backReached_ && mag_ == MAG_IN && i.clip > 0)
                {
                    slide_ = SLIDE_HOME;
                    chambered_ = true;
                    if (e == NO_EVENT)
                        e = EV_SLIDE_HOME;
                    e2 = EV_CHAMBER;
                }
                else if (backReached_ && mag_ == MAG_IN && i.clip <= 0 && i.holdOpen)
                {
                    slide_ = SLIDE_LOCKED;
                    if (e == NO_EVENT)
                        e = EV_SLIDE_LOCK;
                }
                // a locked slide let go before the tug: still locked
                else if (fromLocked_ && !backReached_)
                {
                    slide_ = SLIDE_LOCKED;
                }
                // closes (empty chamber if nothing fed)
                else
                {
                    slide_ = SLIDE_HOME;
                    if (e == NO_EVENT)
                        e = EV_SLIDE_HOME;
                }
            }
        }
        // ---------------------------------------------------------------- dry fire
        const bool coverShut = !i.coverPresent || (!coverHeld_ && cover_ <= .05);
        const bool canFire = chambered_ && slide_ == SLIDE_HOME && mag_ != MAG_SEATING && mag_ != MAG_QUICK &&
                             coverShut && !(i.bolt && (up_ || boltBack_));
        if (triggerEdge && !canFire && e == NO_EVENT)
            e = EV_DRY;
        o.event = e;
        o.event2 = e2;
        o.mag = mag_;
        o.slide = slide_;
        o.chambered = chambered_;
        o.hideMagazine = mag_ == MAG_OUT || mag_ == MAG_HELD || mag_ == MAG_ENTERING;
        o.blockFire = !canFire;
        // The free hand is claimed whenever a part needs it, and whenever it is at the slide (grab it at any time).
        o.claimFree = mag_ == MAG_OUT || mag_ == MAG_HELD || mag_ == MAG_ENTERING || slide_ == SLIDE_HELD ||
                      slide_ == SLIDE_LOCKED || (mag_ == MAG_IN && !chambered_) ||
                      (i.nearSlide && mag_ != MAG_SEATING && mag_ != MAG_QUICK);
        o.holdProxy = mag_ == MAG_HELD || mag_ == MAG_ENTERING;
        o.entering = mag_ == MAG_ENTERING;
        if (i.bolt)
        {
            if (slide_ != SLIDE_HELD)
            {
                o.slideBack = boltBack_ ? i.stroke : 0;
                o.lift = up_ ? 1 : 0;
            }
        }
        else if (slide_ == SLIDE_HELD)
        {
            const double shown = (fromLocked_ ? i.stroke * .9 : 0) + (i.pull > 0 ? i.pull : 0);
            o.slideBack = shown > i.stroke ? i.stroke : shown;
        }
        else
            o.slideBack = slide_ == SLIDE_LOCKED ? i.stroke * .9 : 0;
        o.boltUp = up_;
        o.cover = cover_;
        o.startNative = mag_ == MAG_SEATING || mag_ == MAG_QUICK;
        o.quick = mag_ == MAG_QUICK;
        o.glowMagazine = mag_ == MAG_HELD || mag_ == MAG_ENTERING;
        o.glowSlide = !o.glowMagazine && mag_ == MAG_IN && !chambered_ && i.clip > 0 &&
                      slide_ != SLIDE_HELD && coverShut;
        if (i.bolt)
            o.glowSlide = !o.glowMagazine && slide_ != SLIDE_HELD &&
                          (up_ || boltBack_ || spent_ || (mag_ == MAG_IN && !chambered_ && i.clip > 0));
        o.glowCover = i.coverPresent && !coverHeld_ && !o.glowMagazine &&
                      ((cover_ <= .05 && (mag_ == MAG_OUT || (mag_ == MAG_IN && i.clip <= 0))) ||
                       (cover_ >= .8 && mag_ == MAG_IN && i.clip > 0));
        o.claimFree = o.claimFree || coverHeld_ ||
                      (i.coverPresent && i.nearCover && mag_ != MAG_SEATING && mag_ != MAG_QUICK) ||
                      (i.bolt && (up_ || boltBack_ || spent_));
        return o;
    }
};
}
