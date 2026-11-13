#pragma once
// Build138 physical interaction math (VS2012-compatible; no game/runtime state).
// HIGGS (adamhynek/higgs 93bf67b) and PLANCK (adamhynek/activeragdoll cb54ad3) are BEHAVIOURAL
 
#include "../hands/rigid_grasp.h"
// build158 G: human strength (sourced)
#include "human_strength.h"
namespace gx_physical
{
using namespace gx_physics;

// ---------------------------------------------------------------- HIGGS grasp
// HIGGS places the touched surface on the PALM (TransitionHeld: desired.pos += palmPos - ptPos),
// not at the hand centre. The tracked hand frame origin is the grip/palm centre inside the hand
// model, so the contact lands on the approach line at most `palm` metres from that origin.
// Orientation captured at acquisition is retained; only the translation pivot changes.
inline bool palmPivot(Grasp& g, const Frame& object, const Frame& hand, V contact, double palm)
{
    if (!g.held || !object.rigid() || !hand.rigid() || !finite(contact) || !finite(palm) || palm < 0 ||
        palm > .06)
        return false;
    const V approach = sub(contact, hand.p);
    const double distance = norm(approach);
    if (!finite(distance) || distance > .12)
        return false;
    const double keep = distance > palm ? palm : distance;
    const V offset = distance > 1e-6 ? mul(approach, keep / distance) : V();
    // Object point that must coincide with the hand origin so that `contact` sits at hand.p+offset.
    g.localGrip.p = object.local(sub(contact, offset));
    const V target = hand.local(add(hand.p, offset)), delta = sub(g.handLocal[0], target);
    for (unsigned i = 0; i < 3; ++i)
        g.handLocal[i] = sub(g.handLocal[i], delta);
    return finite(g.localGrip.p);
}
// Hand frame implied by the held object (HIGGS draws the hand from the object, not vice versa).
inline Frame handFromObject(const Grasp& g, const Frame& object)
{
    return g.physicalGrip(object);
}

inline bool orthonormal(Frame& f)
{
    V a = f.a, b = f.b;
    if (!gx_hands::unit(a))
        return false;
    b = sub(b, mul(a, dot(a, b)));
    if (!gx_hands::unit(b))
        return false;
    f.a = a;
    f.b = b;
    f.c = gx_hands::cross(a, b);
    return f.rigid();
}
// t=0 -> from, t=1 -> to. Linear blend of axes then re-orthonormalised: used only for the short
// HIGGS hand lerp-in, where both frames are already close.
inline bool blend(const Frame& from, const Frame& to, double t, Frame& out)
{
    if (!from.rigid() || !to.rigid() || !finite(t))
        return false;
    if (t <= 0)
    {
        out = from;
        return true;
    }
    if (t >= 1)
    {
        out = to;
        return true;
    }
    out.p = add(mul(from.p, 1 - t), mul(to.p, t));
    out.a = add(mul(from.a, 1 - t), mul(to.a, t));
    out.b = add(mul(from.b, 1 - t), mul(to.b, t));
    out.c = add(mul(from.c, 1 - t), mul(to.c, t));
    if (dot(from.a, to.a) + dot(from.b, to.b) + dot(from.c, to.c) < 0)
        return false; // >~120 deg: no lerp shortcut
    return orthonormal(out);
}

// Stiffer HIGGS-like motor. HIGGS drives a grab constraint inside Havok with position motors
 
// this is the equivalent velocity-target motor: desired velocity = feedforward of the desired pose
// (linear AND angular) + error/tau, reached in one step within a finite force/torque budget.
 
struct Limits
{
    // build158 G: force = ONE hand's human ceiling (gx_human158::HAND_N x the menu strength); acceleration and torque are no
    // longer numbers of ours (were 150 m/s2 and 60 N*m, with 800 / 250 / 120 ceilings): a = F / m and the torque is F x lever.
    // responseRate <= 60 (tau >= one step keeps the deadbeat motor from overshooting) and error <= 0.30 m (a glitched hand
    // cannot request an unbounded velocity) are stability / tracking filters and stay.
    double responseRate, force, acceleration, torque,
        error; // responseRate = 1/tau; acceleration, torque: unused since 158
    Limits() : responseRate(30), force(gx_human::HAND_N), acceleration(0), torque(0), error(.30) {}
    bool valid() const
    {
        return finite(responseRate) && responseRate > 0 && responseRate <= 60 && finite(force) && force > 0 &&
               finite(error) && error > 0 && error <= .30;
    }
};
inline Limits scaled(int grabPercent)
{
    Limits out;
    double gain = grabPercent * .01;
    if (!finite(gain) || gain < .25)
        gain = .25;
    if (gain > 2)
        gain = 2;
    out.force *= gain;
    return out;
}
// Desired body motion between two consecutive desired frames (feedforward).
inline bool desiredMotion(const Frame& previous, const Frame& current, V center, double dt, V& linear,
                          V& angular)
{
    linear = V();
    angular = V();
    if (!previous.rigid() || !current.rigid() || !finite(center) || !finite(dt) || dt < .001 || dt > .1)
        return false;
    const V before = previous.world(current.local(center));
    linear = mul(sub(center, before), 1 / dt);
    angular = mul(gx_rigid_grasp::rotationError(previous, current), 1 / dt);
    return finite(linear) && norm(linear) <= gx_motion::GLITCH && finite(angular);
}
struct Packet
{
    bool valid;
    V force, torque;
    double positionError, angleError;
    Packet() : valid(false), positionError(0), angleError(0) {}
};
inline Packet drive(const Grasp& g, const Frame& body, const Frame& hand, V center, V velocity, V angular,
                    const V inertia[3], V gravity, double mass, double dt, unsigned holders,
                    V linearReference, V angularReference, const Limits& limits)
{
    Packet out;
    if (!g.held || !g.token.valid() || !g.epoch || !body.rigid() || !hand.rigid() || !finite(center) ||
        !finite(velocity) || !finite(angular) || !inertia || !finite(gravity) || !finite(linearReference) ||
        !finite(angularReference) || norm(linearReference) > gx_motion::GLITCH || !finite(mass) ||
        mass <= 0 || !finite(dt) || dt <= 0 || dt > .05 || holders < 1 || holders > 2 || !limits.valid())
        return out;
    for (unsigned i = 0; i < 3; ++i)
        if (!finite(inertia[i]))
            return out;
    if (inertia[0].x <= 0 || inertia[1].y <= 0 || inertia[2].z <= 0 ||
        fabs(inertia[0].y - inertia[1].x) > 1e-4 || fabs(inertia[0].z - inertia[2].x) > 1e-4 ||
        fabs(inertia[1].z - inertia[2].y) > 1e-4 ||
        inertia[0].x * inertia[1].y - inertia[0].y * inertia[1].x <= 0 ||
        dot(inertia[0], gx_hands::cross(inertia[1], inertia[2])) <= 0)
        return out;
    const Frame target = gx_rigid_grasp::desired(g, hand);
    if (!target.rigid())
        return out;
    // tau never below one step: dt/tau<=1 keeps the discrete motor deadbeat, not overshooting.
    double tau = 1 / limits.responseRate;
    if (tau < dt)
        tau = dt;
    const V rawError = sub(target.world(body.local(center)), center);
    out.positionError = norm(rawError);
    const V wantVelocity = add(linearReference, mul(limited(rawError, limits.error), 1 / tau));
    const V accel = sub(mul(sub(wantVelocity, velocity), 1 / dt), gravity);
    // build158 G: each hand requests its share and keeps its OWN human ceiling: two hands add (157 divided one budget by the
    // holders, so a second hand added no strength), and no object is refused for its mass (157: > 20 kg dropped).
    const double budget = limits.force;
    out.force = limited(mul(accel, mass / holders), budget);
    const V rotation = gx_rigid_grasp::rotationError(body, target);
    out.angleError = norm(rotation);
    const V wantAngular = add(angularReference, mul(rotation, 1 / tau));
    const V alpha = mul(sub(wantAngular, angular), 1 / dt);
    const double lever = norm(sub(body.world(g.objectLocal[0]), center));
    out.torque =
        limited(mul(add(mul(inertia[0], alpha.x), add(mul(inertia[1], alpha.y), mul(inertia[2], alpha.z))),
                    1.0 / holders),
                gx_human::handTorque(limits.force, lever));
    out.valid = finite(out.force) && finite(out.torque);
    return out;
}
// HIGGS releases when the hand drawn on the object drifts too far from the real hand.
class Deviation
{
    double average_, last_;
    bool seeded_;

  public:
    Deviation() : average_(0), last_(0), seeded_(false) {}
    void reset()
    {
        average_ = 0;
        last_ = 0;
        seeded_ = false;
    }
    double average() const
    {
        return average_;
    }
    // build147 R1-A03: time-weighted (the accepted 60 FPS .8/.2 response at any frame rate); now = QPC seconds.
    bool exceeded(double sample, double now, double limit = .30)
    {
        if (!finite(sample) || sample < 0 || !finite(now))
        {
            reset();
            return true;
        }
        const double dt = now - last_;
        if (!seeded_ || !(dt > 0) || dt > .1)
            average_ = sample;
        else
        {
            const double weight = 1 - pow(.8, dt * 60);
            average_ += (sample - average_) * weight;
        }
        seeded_ = true;
        last_ = now;
        return average_ > limit;
    }
};

// Per-hand held-object motor state: lerp-in start, previous desired pose (feedforward), deviation.
struct HeldMotion
{
    Token token;
    unsigned epoch;
    bool previousValid;
    Frame previous;
    uint64_t startTick;
    Deviation deviation;
    HeldMotion() : epoch(0), previousValid(false), startTick(0) {}
    void reset()
    {
        *this = HeldMotion();
    }
    // A different grasp (token/epoch) restarts the HIGGS hand lerp and the feedforward history.
    void follow(Token t, unsigned e, uint64_t now)
    {
        if (startTick && token.same(t) && epoch == e)
            return;
        reset();
        token = t;
        epoch = e;
        startTick = now ? now : 1;
    }
    double lerp(uint64_t now, double seconds = .15) const
    {
        if (!startTick || now < startTick || !finite(seconds) || seconds <= 0)
            return 0;
        const double t = double(now - startTick) * 1e-6 / seconds;
        return t > 1 ? 1 : t; // build147 R1-A01: now = QPC microseconds
    }
};

// ---------------------------------------------------------------- PLANCK strikes
// PLANCK (no button): a contact is a hit when the hand/weapon closes on the target fast enough
// (punch 2.5 m/s, stab 2, swing 5; plus >=1 m/s real hand speed); trigger held = power attack.
struct StrikeRule
{
    double closing, ratio, room;
    StrikeRule(double c, double r, double h) : closing(c), ratio(r), room(h) {}
    static StrikeRule fist()
    {
        return StrikeRule(2.5, .55, 1.0);
    }
    static StrikeRule weapon()
    {
        return StrikeRule(3.5, .45, 1.0);
    }
};
inline Impact speedStrike(const Hit& hit, V own, V target, const StrikeRule& rule)
{
    Impact out;
    if (!hit.present || !hit.usable() || !finite(own) || !finite(target))
        return out;
    const double room = norm(own);
    if (room < .25 || room > gx_motion::GLITCH)
        return out;
    const V normal = mul(hit.normal, 1 / norm(hit.normal)), relative = sub(own, target);
    const double speed = norm(relative);
    if (speed <= 0 || speed > gx_motion::GLITCH)
        return out;
    out.closingSpeed = -dot(relative, normal);
    if (out.closingSpeed <= .15)
        return out;
    out.direction = mul(own, 1 / room); // actual blow direction, not the NPC's motion
    if (out.closingSpeed >= rule.closing && out.closingSpeed / speed >= rule.ratio && room >= rule.room &&
        room >= rule.closing)
    {
        out.kind = INTENTIONAL_STRIKE;
        out.speed = room;
        out.intensity = gx_strike_curve::fraction(room, rule.closing == 2.5 ? gx_strike_curve::FIST
                                                        : rule.closing == 2 ? gx_strike_curve::KNIFE_STAB
                                                                            : gx_strike_curve::SMALL);
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
inline bool powerHeld(double trigger)
{
    return finite(trigger) && trigger >= .65 && trigger <= 1;
}
// Build157: the shared fraction reaches the native table at the measured reference.
// Trigger state never raises damage or impulse above what motion earns.
inline double strikeDamage(double native, double intensity, bool power)
{
    if (!finite(native) || native <= 0 || native > 200 || !finite(intensity))
        return 0;
    (void)power;
    return gx_strike_curve::damage(native, intensity);
}
inline double strikeImpulse(double intensity, bool power)
{
    if (!finite(intensity))
        return 0;
    if (intensity < 0)
        intensity = 0;
    if (intensity > 1)
        intensity = 1;
    (void)power;
    const double i = 4 + 8 * intensity;
    return i > 12 ? 12 : i; // native sPhysicalMeleeInfo bound
}
}
