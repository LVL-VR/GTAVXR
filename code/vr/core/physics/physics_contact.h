#pragma once
// Owned policy/math for native WorldProbe and CPhysical force consumers.
// This never integrates an object, creates a world, or writes tracking.
#include "../body/avatar_pose.h"
#include "human_motion.h"
#include "strike_curve.h"
#include "../hands/hand_basis.h"
#include <stdint.h>
#include <math.h>
#include <string.h>

namespace gx_physics
{
typedef gx_hands::V V;
using gx_hands::add;
using gx_hands::sub;
using gx_hands::mul;
using gx_hands::dot;
inline double norm(V a)
{
    return sqrt(dot(a, a));
}
inline bool finite(V a)
{
    return gx_avatar::finite(a);
}
inline bool finite(double a)
{
    return _finite(a) != 0;
}
inline V limited(V v, double bound)
{
    double n = norm(v);
    return n > bound && n > 0 ? mul(v, bound / n) : v;
}

struct Token
{
    unsigned level, generation, component;
    Token() : level(65535), generation(0), component(0) {}
    Token(unsigned l, unsigned g, unsigned c) : level(l), generation(g), component(c) {}
     
    bool valid() const
    {
        return level < 65535 && generation <= 65535 && component <= 65535;
    }
    bool same(const Token& t) const
    {
        return level == t.level && generation == t.generation && component == t.component;
    }
    bool sameBody(const Token& t) const
    {
        return level == t.level && generation == t.generation;
    }
    uint64_t id() const
    {
        return valid() ? uint64_t(level + 1) | (uint64_t(generation) << 16) | (uint64_t(component) << 32) : 0;
    }
};
struct Hit
{
    bool present;
    double fraction, depth;
    V point, normal;
    Token token;
    unsigned part;
    uint64_t material;
    Hit() : present(false), fraction(1), depth(0), part(0), material(0) {}
    bool usable() const
    {
        return !present || (finite(fraction) && fraction >= 0 && fraction <= 1 && finite(depth) &&
                            finite(point) && finite(normal) && norm(normal) > .5 && norm(normal) < 1.5);
    }
};
enum Status
{
    DISABLED,
    UNSEEDED,
    CLEAR,
    CONTACT_SOLVED,
    INITIAL_OVERLAP,
    QUERY_FAILED,
    INVALID_SAMPLE,
    DISCONTINUITY,
    ITERATION_LIMIT
};
struct Sample
{
    unsigned epoch;
    uint64_t frame, generation;
    double realTime, simDt;
    V localPosition, worldPosition, rootPosition;
    bool tracked, eligible, recenter;
    Sample()
        : epoch(0), frame(0), generation(0), realTime(0), simDt(0), tracked(false), eligible(false),
          recenter(false)
    {
    }
};
struct Solved
{
    Status status;
    bool valid;
    unsigned epoch;
    uint64_t frame, generation;
    V position, roomVelocity;
    Hit hit;
    unsigned queries;
    Solved() : status(DISABLED), valid(false), epoch(0), frame(0), generation(0), queries(0) {}
};

// Backend.sweep(from,to,radius,Hit&) and Backend.overlap(at,radius,bool&)
 
class Hand
{
    bool ready_;
    Sample previous_;
    V position_;
    Solved last_;

  public:
    Hand() : ready_(false) {}
    void reset()
    {
        ready_ = false;
        previous_ = Sample();
        position_ = V();
        last_ = Solved();
    }
    const Solved& last() const
    {
        return last_;
    }
    template <class Backend>
    Solved update(const Sample& s, Backend& backend, double radius = .045, double skin = .0015)
    {
        Solved out;
        out.epoch = s.epoch;
        out.frame = s.frame;
        out.generation = s.generation;
        if (!s.eligible || !s.tracked || !s.epoch || !s.frame || !s.generation)
        {
            reset();
            return last_ = out;
        }
        if (!finite(s.realTime) || !finite(s.simDt) || s.simDt < 0 || !finite(s.localPosition) ||
            !finite(s.worldPosition) || !finite(s.rootPosition) || !finite(radius) || radius < .005 ||
            radius > 1 || !finite(skin) || skin < 0 || skin > .01)
        {
            reset();
            out.status = INVALID_SAMPLE;
            return last_ = out;
        }
        if (ready_ && !s.recenter && s.epoch == previous_.epoch && s.frame == previous_.frame &&
            s.generation == previous_.generation)
        {
            return last_;
        }
        const double dt = ready_ ? s.realTime - previous_.realTime : 0;
        const bool continuity =
            ready_ && s.epoch == previous_.epoch && !s.recenter && s.frame > previous_.frame &&
            s.generation >= previous_.generation && dt >= .001 && dt <= .10 &&
            norm(sub(s.localPosition, previous_.localPosition)) <= gx_motion::GLITCH * dt &&
            norm(sub(s.rootPosition, previous_.rootPosition)) <= .35 &&
            norm(sub(s.worldPosition, previous_.worldPosition)) <=
                gx_motion::GLITCH * dt + norm(sub(s.rootPosition, previous_.rootPosition));
        if (ready_ && !continuity)
        {
            reset();
            out.status = DISCONTINUITY;
            return last_ = out;
        }
        if (s.recenter)
        {
            reset();
            out.status = DISCONTINUITY;
            return last_ = out;
        }
        bool overlap = false;
        if (!ready_)
        {
            ++out.queries;
            if (!backend.overlap(s.worldPosition, radius, overlap))
            {
                out.status = QUERY_FAILED;
                return last_ = out;
            }
            if (overlap)
            {
                out.status = INITIAL_OVERLAP;
                return last_ = out;
            }
            ready_ = true;
            position_ = s.worldPosition;
            previous_ = s;
            out.status = CLEAR;
            out.valid = true;
            out.position = position_;
            return last_ = out;
        }
        // Use world-space relative-to-root displacement for direction; speed is
        // bounded independently in raw roomspace, never divided by game time.
        out.roomVelocity = mul(
            sub(sub(s.worldPosition, previous_.worldPosition), sub(s.rootPosition, previous_.rootPosition)),
            1 / dt);
        // Large yaw/recenter warps can be plausible world deltas but are not a
        // physical strike. Root transport is not included in this velocity.
        if (norm(out.roomVelocity) > gx_motion::GLITCH)
        {
            reset();
            out.status = DISCONTINUITY;
            return last_ = out;
        }
        V current = position_, destination = s.worldPosition;
        bool touched = false, completed = false;
        for (unsigned i = 0; i < 3; ++i)
        {
            V delta = sub(destination, current);
            double distance = norm(delta);
            if (distance < 1e-7)
            {
                completed = true;
                break;
            }
            Hit hit;
            ++out.queries;
            if (!backend.sweep(current, destination, radius, hit) || !hit.usable())
            {
                reset();
                out.status = QUERY_FAILED;
                return last_ = out;
            }
            if (!hit.present)
            {
                current = destination;
                completed = true;
                break;
            }
            if (!touched)
                out.hit = hit;
            touched = true;
            V normal = mul(hit.normal, 1 / norm(hit.normal));
            const double fraction = hit.fraction > skin / distance ? hit.fraction - skin / distance : 0;
            current = add(current, mul(delta, fraction));
            V remaining = sub(destination, current);
            double inward = dot(remaining, normal);
            if (inward < 0)
                remaining = sub(remaining, mul(normal, inward));
            destination = add(current, remaining);
            if (norm(remaining) < 1e-7)
            {
                completed = true;
                break;
            }
        }
        // Never publish a penetrating endpoint even when a native cast reports
        // an edge/t=0 hit. Failure holds the last proven free point, not tracker.
        ++out.queries;
        if (!backend.overlap(current, radius, overlap))
        {
            reset();
            out.status = QUERY_FAILED;
            return last_ = out;
        }
        if (overlap)
        {
            out.status = QUERY_FAILED;
            out.position = position_;
            previous_ = s;
            return last_ = out;
        }
        position_ = current;
        previous_ = s;
        out.valid = true;
        out.position = current;
        out.status = completed ? (touched ? CONTACT_SOLVED : CLEAR) : ITERATION_LIMIT;
        return last_ = out;
    }
};

// Released first, then pressed. Identity/tracking/ownership loss while held
// requires a genuine neutral input; moving a held grip into a candidate cannot
// acquire and a frame gap must not produce a throw/re-grab.
class GripLatch
{
    bool neutral_, down_;

  public:
    GripLatch() : neutral_(false), down_(true) {}
    void reset()
    {
        neutral_ = false;
        down_ = true;
    }
    bool update(double value, bool eligible)
    {
        if (!eligible || !finite(value))
        {
            reset();
            return false;
        }
        if (value < .45)
        {
            neutral_ = true;
            down_ = false;
            return false;
        }
        if (value >= .65)
        {
            bool edge = neutral_ && !down_;
            down_ = true;
            return edge;
        }
        return false;
    }
    bool released(double value) const
    {
        return finite(value) && value < .45;
    }
};

struct Frame
{
    V a, b, c, p;
    Frame() : a(1, 0, 0), b(0, 1, 0), c(0, 0, 1) {}
    bool rigid() const
    {
        return finite(a) && finite(b) && finite(c) && finite(p) && fabs(dot(a, a) - 1) < .01 &&
               fabs(dot(b, b) - 1) < .01 && fabs(dot(c, c) - 1) < .01 && fabs(dot(a, b)) < .01 &&
               fabs(dot(a, c)) < .01 && fabs(dot(b, c)) < .01 && dot(gx_hands::cross(a, b), c) > .99;
    }
    V world(V v) const
    {
        return add(p, add(mul(a, v.x), add(mul(b, v.y), mul(c, v.z))));
    }
    V local(V v) const
    {
        v = sub(v, p);
        return V(dot(v, a), dot(v, b), dot(v, c));
    }
};
// A native enclosing sphere is orientation-independent. Sweep its centroid,
// then translate the same rigid aim matrix. All weapon consumers share this
// result; no rotational clipping correction can rewrite the tracker or aim.
struct WeaponSolved
{
    bool valid;
    Frame frame;
    Solved contact;
    WeaponSolved() : valid(false) {}
};
class Weapon
{
    Hand center_;
    uint64_t object_;
    unsigned epoch_;
    double radius_;
    V centroid_;

  public:
    Weapon() : object_(0), epoch_(0), radius_(0) {}
    void reset()
    {
        center_.reset();
        object_ = epoch_ = 0;
        radius_ = 0;
        centroid_ = V();
    }
    template <class Backend>
    WeaponSolved update(uint64_t object, const Sample& sample, const Frame& desired, V localCentroid,
                        double radius, Backend& backend)
    {
        WeaponSolved out;
        if (!object || !desired.rigid() || !finite(localCentroid) || !finite(radius) || radius < .005 ||
            radius > 1)
        {
            reset();
            return out;
        }
        if (object != object_ || sample.epoch != epoch_ || fabs(radius - radius_) > 1e-6 ||
            norm(sub(localCentroid, centroid_)) > 1e-6)
        {
            reset();
            object_ = object;
            epoch_ = sample.epoch;
            radius_ = radius;
            centroid_ = localCentroid;
        }
        Sample center = sample;
        center.worldPosition = desired.world(localCentroid);
        out.contact = center_.update(center, backend, radius);
        if (!out.contact.valid)
            return out;
        out.frame = desired;
        out.frame.p = add(desired.p, sub(out.contact.position, center.worldPosition));
        out.valid = out.frame.rigid();
        return out;
    }
};
struct Grasp
{
    bool held;
    Token token;
    unsigned epoch;
    V objectLocal[3], handLocal[3];
    Frame localGrip;
    Grasp() : held(false), epoch(0) {}
    void release()
    {
        held = false;
        token = Token();
        epoch = 0;
    }
    bool acquire(Token t, unsigned e, const Frame& object, const Frame& hand, V actualContact,
                 double span = .04)
    {
        if (!t.valid() || !e || !object.rigid() || !hand.rigid() || !finite(actualContact) || !finite(span) ||
            span < .001 || span > .08 || norm(sub(actualContact, hand.p)) > .12)
            return false;
        // Native contact defines the grasp; no center snap or universal offset.
        V points[3] = {actualContact, add(actualContact, mul(object.a, span)),
                       add(actualContact, mul(object.b, span))};
        for (unsigned i = 0; i < 3; ++i)
        {
            objectLocal[i] = object.local(points[i]);
            handLocal[i] = hand.local(points[i]);
        }
        localGrip.p = object.local(hand.p);
        localGrip.a = V(dot(hand.a, object.a), dot(hand.a, object.b), dot(hand.a, object.c));
        localGrip.b = V(dot(hand.b, object.a), dot(hand.b, object.b), dot(hand.b, object.c));
        localGrip.c = V(dot(hand.c, object.a), dot(hand.c, object.b), dot(hand.c, object.c));
        token = t;
        epoch = e;
        held = true;
        return true;
    }
    Frame physicalGrip(const Frame& object) const
    {
        Frame result;
        result.p = object.world(localGrip.p);
        result.a = sub(object.world(localGrip.a), object.p);
        result.b = sub(object.world(localGrip.b), object.p);
        result.c = sub(object.world(localGrip.c), object.p);
        return result;
    }
};
struct ForcePacket
{
    bool valid;
    V point[3], force[3], net, torque;
    double dt;
    ForcePacket() : valid(false), dt(0) {}
};
// A bounded drive of the EXISTING native body. Native collisions/hinges win.
// Native integration consumes F*simDt; no velocity set or release impulse.
inline ForcePacket drive(const Grasp& grasp, const Frame& object, const Frame& hand, V velocity,
                         V angularVelocity, double mass, double simDt, double handBudget = 120,
                         double accelerationLimit = 20, double torqueLimit = 8, const V* nativeCenter = NULL,
                         const V* nativePointVelocity = NULL)
{
    ForcePacket out;
    if (!grasp.held || !object.rigid() || !hand.rigid() || !finite(velocity) || !finite(angularVelocity) ||
        !finite(mass) || mass <= 0 || mass > 20 || !finite(simDt) || simDt <= 0 || simDt > .05 ||
        !finite(handBudget) || handBudget <= 0 || !finite(accelerationLimit) || accelerationLimit <= 0 ||
        !finite(torqueLimit) || torqueLimit <= 0)
        return out;
    const V center = nativeCenter ? *nativeCenter : object.p;
    if (!finite(center))
        return out;
    const double totalBudget = mass * accelerationLimit < handBudget ? mass * accelerationLimit : handBudget;
    const double lever = norm(sub(grasp.objectLocal[1], grasp.objectLocal[0]));
    const double torqueBudget = lever > 0 ? torqueLimit / (6 * lever) : totalBudget / 3;
    const double pointBudget = torqueBudget < totalBudget / 3 ? torqueBudget : totalBudget / 3;
    for (unsigned i = 0; i < 3; ++i)
    {
        out.point[i] = object.world(grasp.objectLocal[i]);
        V target = hand.world(grasp.handLocal[i]);
        V error = sub(target, out.point[i]);
        if (norm(error) > .30)
            return ForcePacket();
        V pointVelocity = nativePointVelocity
                              ? nativePointVelocity[i]
                              : add(velocity, gx_hands::cross(angularVelocity, sub(out.point[i], center)));
        if (!finite(pointVelocity))
            return ForcePacket();
        // Real target motion is represented by pose error; explicit damping
        // prevents high kinetic gain without making a second integrator.
        out.force[i] =
            limited(sub(mul(error, mass * 90 / 3), mul(pointVelocity, mass * 12 / 3)), pointBudget);
        out.net = add(out.net, out.force[i]);
        out.torque = add(out.torque, gx_hands::cross(sub(out.point[i], center), out.force[i]));
    }
    // Off-center native grasps can have long lever arms. Apply one common
    // scale so both the force and torque budgets are preserved together.
    double scale = 1, net = norm(out.net), torque = norm(out.torque);
    if (net > totalBudget)
        scale = totalBudget / net;
    if (torque > torqueLimit && torqueLimit / torque < scale)
        scale = torqueLimit / torque;
    if (scale < 1)
    {
        for (unsigned i = 0; i < 3; ++i)
            out.force[i] = mul(out.force[i], scale);
        out.net = mul(out.net, scale);
        out.torque = mul(out.torque, scale);
    }
    out.valid = finite(out.net) && finite(out.torque);
    out.dt = simDt;
    return out;
}

// Mirrors the native ApplyForce assertion's coordinate convention without
// invoking an assertion-producing helper. Collider and entity origins may
// differ. The multiplier is of the native radius, NOT a distance in metres.
// Supplied by the integration host; excluded data/code stay outside the core.
bool nativeForcePoint(V point, V entityOrigin, V assertionOrigin, V worldCentroid, double radius);
enum ContactKind
{
    ACCIDENTAL,
    PUSH,
    INTENTIONAL_STRIKE
};
// speed: the strike metric the class gate used (build158 NPC damage curve); 0 when not a strike.
struct Impact
{
    ContactKind kind;
    double closingSpeed, intensity, speed;
    V direction;
    Impact() : kind(ACCIDENTAL), closingSpeed(0), intensity(0), speed(0) {}
};
inline Impact classify(const Hit& hit, V physicalVelocity, V targetVelocity, bool deliberateFist,
                       gx_strike_curve::Class strikeClass = gx_strike_curve::FIST)
{
    Impact out;
    if (!hit.present || !hit.usable() || !finite(physicalVelocity) || !finite(targetVelocity))
        return out;
    V velocity = sub(physicalVelocity, targetVelocity), normal = mul(hit.normal, 1 / norm(hit.normal));
    const double speed = norm(velocity);
    if (speed <= 0 || speed > gx_motion::GLITCH)
        return out;
    out.closingSpeed = -dot(velocity, normal);
    if (out.closingSpeed <= .15)
        return out;
    out.direction = mul(velocity, 1 / speed);
    if (deliberateFist && norm(physicalVelocity) >= gx_strike_curve::gate(strikeClass) &&
        out.closingSpeed >= gx_strike_curve::gate(strikeClass) && out.closingSpeed / speed >= .55)
    {
        out.kind = INTENTIONAL_STRIKE;
        out.intensity = gx_strike_curve::fraction(norm(physicalVelocity), strikeClass);
    }
    else if (out.closingSpeed >= .25)
    {
        out.kind = PUSH;
        out.intensity = out.closingSpeed / 2;
        if (out.intensity > 1)
            out.intensity = 1;
    }
    return out;
}
class ImpactGate
{
    Token target_;
    unsigned epoch_;
    double last_;
    bool touching_;

  public:
    ImpactGate() : epoch_(0), last_(-1), touching_(false) {}
    void reset()
    {
        target_ = Token();
        epoch_ = 0;
        last_ = -1;
        touching_ = false;
    }
    void separate(unsigned epoch)
    {
        if (epoch_ && epoch_ != epoch)
            reset();
        epoch_ = epoch;
        touching_ = false;
    }
    bool update(Token target, unsigned epoch, double realTime, ContactKind kind, bool touching)
    {
        if (!target.valid() || !epoch || !finite(realTime))
        {
            reset();
            return false;
        }
        if (epoch_ && epoch != epoch_)
        {
            reset();
            epoch_ = epoch;
            target_ = target;
            touching_ = touching;
            return false;
        }
        const bool same = target_.same(target);
        bool fire = kind == INTENTIONAL_STRIKE && touching && (!same || !touching_) &&
                    (last_ < 0 || realTime - last_ >= .25);
        epoch_ = epoch;
        target_ = target;
        touching_ = touching;
        if (fire)
            last_ = realTime;
        return fire;
    }
};

enum Action
{
    NO_ACTION,
    GRAB,
    RELEASE,
    TWO_HAND_GRAB,
    PUSH_CONTACT,
    STRIKE_CONTACT
};
struct Candidate
{
    Action action;
    Token target;
    unsigned hand, epoch;
    uint64_t frame, generation;
    bool held, canExecute, known;
    Candidate()
        : action(NO_ACTION), hand(0), epoch(0), frame(0), generation(0), held(false), canExecute(false),
          known(false)
    {
    }
};
}
