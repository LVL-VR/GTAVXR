#pragma once
 
// Reuse the pinned guarded palm estimator; no new smoothing/filter coefficients.
#include "physical_hand_motion.h"
namespace gx_throw
{
using gx_physics::V;
enum Family
{
    OTHER,
    GRENADE,
    SMOKE,
    BZ,
    FLARE,
    MOLOTOV,
    STICKY,
    PROX,
    PIPE
};
enum Element
{
    NO_ELEMENT,
    PIN,
    IGNITION
};
enum Path
{
    TRIGGER_PATH,
    ELEMENT_PATH
};
struct Facts
{
    Family family;
    Element element;
    double fuse;
    bool cook, dropWhenCooked;
    Facts(Family f = OTHER, Element e = NO_ELEMENT, double t = 0, bool c = false, bool d = false)
        : family(f), element(e), fuse(t), cook(c), dropWhenCooked(d)
    {
    }
};
// Supplied by the integration host; excluded data/code stay outside the core.
Facts facts(unsigned hash);
inline const char* name(Family f)
{
    switch (f)
    {
    case GRENADE:
        return "grenade";
    case SMOKE:
        return "smoke";
    case BZ:
        return "bz";
    case FLARE:
        return "flare";
    case MOLOTOV:
        return "molotov";
    case STICKY:
        return "sticky";
    case PROX:
        return "prox";
    case PIPE:
        return "pipe";
    default:
        return "other";
    }
}
struct Launch
{
    bool valid;
    V velocity, direction;
    double measured, capped, limit;
    Launch() : valid(false), direction(0, 0, -1), measured(0), capped(0), limit(0) {}
};
inline Launch launch(V measured, double range, double gravity, double coefficient)
{
    Launch out;
    if (!gx_physics::finite(measured) || !gx_physics::finite(range) || range <= 0 ||
        !gx_physics::finite(gravity) || gravity <= 0 || !gx_physics::finite(coefficient) || coefficient < 0)
        return out;
    out.measured = gx_physics::norm(measured);
    out.limit = gx_motion::GLITCH;
    if (!gx_physics::finite(out.measured) || out.measured > gx_motion::GLITCH)
        return out;
    out.capped = out.measured;
    out.velocity = measured;
    if (out.measured > 0)
    {
        out.direction = gx_physics::mul(measured, 1 / out.measured);
    }
    out.valid = true;
    return out; // Zero is a real zero-speed native drop, never a minimum impulse.
}
struct Key
{
    uint64_t actor, instance;
    unsigned epoch, activation, hash;
    Key() : actor(0), instance(0), epoch(0), activation(0), hash(0) {}
    bool valid() const
    {
        return actor && instance && epoch && activation && hash;
    }
    bool same(const Key& k) const
    {
        return actor == k.actor && instance == k.instance && epoch == k.epoch && activation == k.activation &&
               hash == k.hash;
    }
};
struct Input
{
    Key key;
    bool eligible, freeEligible, anchorValid, hold;
    double trigger, grip, freeGrip;
    V relative;
    gx_hand_motion::Publication motion;
    unsigned hand;
     
    Input()
        : eligible(false), freeEligible(false), anchorValid(false), hold(false), trigger(0), grip(0),
          freeGrip(0), hand(0)
    {
    }
};
struct Event
{
    bool arm, release, reserve;
    Path path;
    V velocity;
    uint64_t serial;
    Event() : arm(false), release(false), reserve(false), path(TRIGGER_PATH), serial(0) {}
};
class State
{
    Key key_;
    gx_hand_motion::History history_;
    gx_hand_motion::Stamp stamp_;
    bool neutral_, freeNeutral_, freeDown_, captured_, armed_, triggerDown_, gripDown_;
    V capture_;
    Path path_;
    uint64_t serial_;
    // Claude integration amendment (build149): recent VALID palm velocities. A throw's speed is its peak over the last
    // RECENT_S seconds (the hand decelerates as the trigger opens), and one invalid sample (a >100 ms hitch, a >gx_motion::GLITCH m/s
    // wrist snap, a tracking hiccup) must NOT disarm an armed, cooking throwable (it would then explode in the hand).
    enum
    {
        RECENT = 32
    };
    V recent_[RECENT];
    double recentTime_[RECENT];
    unsigned recentCount_;
    void clearRecent()
    {
        recentCount_ = 0;
        for (unsigned n = 0; n < RECENT; ++n)
        {
            recent_[n] = V();
            recentTime_[n] = 0;
        }
    }
    void pushRecent(V v, double t)
    {
        for (unsigned n = RECENT - 1; n > 0; --n)
        {
            recent_[n] = recent_[n - 1];
            recentTime_[n] = recentTime_[n - 1];
        }
        recent_[0] = v;
        recentTime_[0] = t;
        if (recentCount_ < RECENT)
            ++recentCount_;
    }

  public:
    static double recentWindow()
    {
        return .10;
    }
    // Peak-magnitude valid sample within the window ending at `now`; none -> zero (a real native drop, never invented).
    V peak(double now) const
    {
        V best;
        double bestNorm = -1;
        for (unsigned n = 0; n < recentCount_; ++n)
        {
            const double age = now - recentTime_[n];
            if (!gx_physics::finite(age) || age < 0 || age > recentWindow())
                continue;
            const double m = gx_physics::norm(recent_[n]);
            if (gx_physics::finite(m) && m > bestNorm)
            {
                bestNorm = m;
                best = recent_[n];
            }
        }
        return best;
    }
    State() : serial_(0)
    {
        cancel();
    }
    void cancel()
    {
        key_ = Key();
        history_.unknown();
        stamp_ = gx_hand_motion::Stamp();
        neutral_ = freeNeutral_ = false;
        freeDown_ = captured_ = armed_ = triggerDown_ = gripDown_ = false;
        capture_ = V();
        path_ = TRIGGER_PATH;
        clearRecent();
    }
    bool armed() const
    {
        return armed_;
    }
    Event update(const Input& i)
    {
        Event out;
        if (i.hand > 1 || !i.eligible || !i.key.valid() || !(i.motion.mask & (1u << i.hand)) ||
            !gx_physics::finite(i.trigger) || i.trigger < 0 || i.trigger > 1 || !gx_physics::finite(i.grip) ||
            i.grip < 0 || i.grip > 1 || !gx_physics::finite(i.freeGrip) || i.freeGrip < 0 || i.freeGrip > 1)
        {
             
            // of the same throwable; a gap sample never releases. Real context loss (menu, vehicle, death, weapon change) still cancels.
            if (armed_ && i.hold && i.hand <= 1 && i.key.valid() && key_.same(i.key))
            {
                history_.unknown();
                stamp_ = gx_hand_motion::Stamp();
                captured_ = false;
                return out;
            }
            cancel();
            return out;
        }
        if (!key_.same(i.key))
        {
            cancel();
            key_ = i.key;
        }
         
        if (stamp_.same(i.motion.stamp))
        {
            if (!i.freeEligible || !i.anchorValid || i.freeGrip < .2 || !gx_physics::finite(i.relative) ||
                gx_physics::norm(i.relative) > .30)
                captured_ = false;
            out.reserve = captured_ && i.freeGrip >= .2 && i.freeEligible;
            return out;
        } // Duplicate reads can revoke ownership, never create another edge.
        const gx_hand_motion::Velocity velocity =
            history_.update(i.motion.stamp, i.motion.hand[i.hand], true);
        stamp_ = i.motion.stamp;
        if (velocity.valid)
            pushRecent(velocity.own, i.motion.stamp.time);
        else if (!armed_)
        {
            neutral_ = freeNeutral_ = false;
            freeDown_ = captured_ = triggerDown_ = gripDown_ = false;
            return out;
        }
        // Armed + invalid sample: keep the arm and its release edge (Claude amendment above).
        if (i.trigger < .2)
            neutral_ = true;
        if (i.freeGrip < .2)
        {
            freeNeutral_ = true;
            freeDown_ = false;
            captured_ = false;
        }
        const bool freeEdge = freeNeutral_ && !freeDown_ && i.freeGrip >= .65;
        if (i.freeGrip >= .65)
        {
            freeDown_ = true;
            freeNeutral_ = false;
        }
        const Facts f = facts(i.key.hash);
        if (!i.freeEligible || !i.anchorValid || !gx_physics::finite(i.relative))
            captured_ = false;
        const double distance = gx_physics::norm(i.relative);
        if (captured_ && distance > .30)
            captured_ = false; // Leaving before a valid pull cancels acquisition.
        if (!armed_ && neutral_ && freeEdge && i.freeEligible && i.anchorValid && f.element != NO_ELEMENT &&
            distance <= .12)
        {
            captured_ = true;
            capture_ = i.relative;
        }
        if (!armed_ && neutral_ && i.trigger >= .55)
        {
            armed_ = true;
            path_ = TRIGGER_PATH;
            out.arm = true;
        }
        if (!armed_ && captured_ && i.freeGrip >= .65 && distance <= .30 &&
            gx_physics::norm(gx_physics::sub(i.relative, capture_)) >= .08 &&
            distance >= gx_physics::norm(capture_) + .06)
        {
            armed_ = true;
            path_ = ELEMENT_PATH;
            out.arm = true;
        }
        // The holder must actually squeeze a release control. Starting with an
        // open hand during pin pull never generates a falling edge.
        if (armed_)
        {
            const bool release = (triggerDown_ && i.trigger < .2) || (gripDown_ && i.grip < .2);
            if (i.trigger >= .55)
                triggerDown_ = true;
            if (path_ == ELEMENT_PATH && i.grip >= .65)
                gripDown_ = true;
            if (release)
            {
                out.release = true;
                out.path = path_;
                out.velocity = peak(i.motion.stamp.time);
                out.serial = ++serial_;
                armed_ = false;
                neutral_ = false;
                captured_ = false;
                triggerDown_ = gripDown_ = false;
            }
        }
        out.reserve = captured_ && i.freeGrip >= .2 && i.freeEligible;
        return out;
    }
};
// One consumed edge permits at most one native attempt, even after failure.
class Once
{
    uint64_t used_;

  public:
    Once() : used_(0) {}
    bool take(uint64_t serial)
    {
        if (!serial || serial == used_)
            return false;
        used_ = serial;
        return true;
    }
};
}
