#pragma once
 
// Queries observe tracking; they never solve or replace a tracked display pose.
#include "physics_interaction.h"
// build158 G: human strength (sourced)
#include "human_strength.h"

namespace gx_tracked_physics
{
using gx_physics::V;
using gx_physics::Frame;
using gx_physics::Sample;
using gx_physics::Solved;
using gx_physics::Hit;
using gx_physics::finite;
using gx_physics::norm;
using gx_physics::add;
using gx_physics::sub;
using gx_physics::mul;
using gx_physics::limited;

enum HandPose
{
    NATIVE_POSE,
    TRACKED_POSE,
    WHEEL_POSE,
    WEAPON_POSE
};
inline HandPose handPose(bool wheel, bool weapon, bool tracked, bool eligible, bool valid)
{
    if (wheel)
        return WHEEL_POSE;
    if (weapon)
        return WEAPON_POSE;
    return tracked && eligible && valid ? TRACKED_POSE : NATIVE_POSE;
}

struct Observation
{
    Solved contact;
    bool trackingValid;
    V targetVelocity; // relative-to-root velocity in SIMULATION seconds, not strike speed.
    Observation() : trackingValid(false) {}
};

// Same native overlap/sweep backend as the proven hand query. Unlike Hand's
// stop/slide solver, each segment starts at the previous RAW tracking sample.
// A native-query failure revokes contact only. A held body still needs its own
// exact instance/generation/component/geometry/ownership checks before a force.
class Contact
{
    bool ready_;
    Sample previous_;
    Observation last_;

  public:
    Contact() : ready_(false) {}
    void reset()
    {
        ready_ = false;
        previous_ = Sample();
        last_ = Observation();
    }
    const Observation& last() const
    {
        return last_;
    }
    template <class Backend> Observation update(const Sample& s, Backend& backend, double radius = .045)
    {
        Observation out;
        Solved& c = out.contact;
        c.epoch = s.epoch;
        c.frame = s.frame;
        c.generation = s.generation;
        c.position = s.worldPosition;
        if (!s.eligible || !s.tracked || !s.epoch || !s.frame || !s.generation)
        {
            reset();
            return last_ = out;
        }
        if (!finite(s.realTime) || !finite(s.simDt) || s.simDt <= 0 || s.simDt > .05 ||
            !finite(s.localPosition) || !finite(s.worldPosition) || !finite(s.rootPosition) ||
            !finite(radius) || radius < .005 || radius > 1)
        {
            reset();
            c.status = gx_physics::INVALID_SAMPLE;
            return last_ = out;
        }
        if (s.recenter)
        {
            reset();
            c.status = gx_physics::DISCONTINUITY;
            return last_ = out;
        }
        if (ready_ && s.epoch == previous_.epoch && s.frame == previous_.frame &&
            s.generation == previous_.generation)
            return last_;
        const double physicalDt = ready_ ? s.realTime - previous_.realTime : 0;
        if (ready_ && (s.epoch != previous_.epoch || s.frame <= previous_.frame ||
                       s.generation < previous_.generation || physicalDt < .001 || physicalDt > .10 ||
                       norm(sub(s.localPosition, previous_.localPosition)) > gx_motion::GLITCH * physicalDt ||
                       norm(sub(s.rootPosition, previous_.rootPosition)) > .35 ||
                       norm(sub(s.worldPosition, previous_.worldPosition)) > .50))
        {
            reset();
            c.status = gx_physics::DISCONTINUITY;
            return last_ = out;
        }
        const bool seeded = ready_;
        const V from = seeded ? previous_.worldPosition : s.worldPosition;
        if (seeded)
        {
            const V relative = sub(sub(s.worldPosition, previous_.worldPosition),
                                   sub(s.rootPosition, previous_.rootPosition));
            c.roomVelocity = mul(relative, 1 / physicalDt);
            out.targetVelocity = mul(relative, 1 / s.simDt);
            // build158 G: the tracking glitch filter (60 m/s, kept by the user) instead of 12 m/s of ours: a hand swung faster
            // than 12 m/s reset its contacts and could drop a held object mid-swing
            if (!finite(c.roomVelocity) || !finite(out.targetVelocity) ||
                norm(c.roomVelocity) > gx_motion::GLITCH)
            {
                reset();
                c.status = gx_physics::DISCONTINUITY;
                return last_ = out;
            }
            // Slow motion changes simulation time, not tracking validity.
            // Saturate only the finite force reference; real strike velocity
            // remains c.roomVelocity/the separately qualified raw hand history.
            out.targetVelocity = limited(out.targetVelocity, 20);
        }
        // Store raw tracking even if the collision query is unavailable/inside.
        previous_ = s;
        ready_ = true;
        out.trackingValid = true;
        ++c.queries;
        if (!seeded || norm(sub(s.worldPosition, from)) < 1e-7)
        {
            bool occupied = false;
            if (!backend.overlap(s.worldPosition, radius, occupied))
            {
                c.status = gx_physics::QUERY_FAILED;
                return last_ = out;
            }
            c.valid = true;
            c.status = occupied ? gx_physics::INITIAL_OVERLAP : gx_physics::CLEAR;
            // Occupancy alone has no invented component, point or normal.
            return last_ = out;
        }
        Hit hit;
        if (!backend.sweep(from, s.worldPosition, radius, hit) || !hit.usable())
        {
            c.status = gx_physics::QUERY_FAILED;
            return last_ = out;
        }
        c.hit = hit;
        c.valid = true;
        // Legacy CONTACT_SOLVED tag means observed contact here, NOT a solved
        // free-space pose. c.position remains exactly the raw requested point.
        c.status = hit.present ? gx_physics::CONTACT_SOLVED : gx_physics::CLEAR;
        return last_ = out;
    }
};

// Finite candidate motor limits, not Havok constants or an acceptance claim.
// 40m/s^2 remains below CPhysical's source-verified260m/s^2 assertion.
struct DriveLimits
{
    double responseRate, force, acceleration, torque,
        error; // error is finite motor saturation, NOT release distance.
    DriveLimits() : responseRate(15), force(240), acceleration(40), torque(40), error(.30) {}
    bool valid() const
    {
        return finite(responseRate) && responseRate > 0 && responseRate <= 30 && finite(force) && force > 0 &&
               force <= 600 && finite(acceleration) && acceleration > 0 && acceleration <= 60 &&
               finite(torque) && torque > 0 && torque <= 80 && finite(error) && error > 0 && error <= .30;
    }
};

// Native bind lengths supply the escape distance. A resisted object cannot
// retain a hand across more than one anatomical arm plus a small margin.
// This only revokes its force/ownership: it NEVER changes the tracked pose.
inline bool withinReach(const gx_physics::Grasp& grasp, const Frame& object, const Frame& hand,
                        double armLength, double margin = .10)
{
    if (!grasp.held || !grasp.token.valid() || !grasp.epoch || !object.rigid() || !hand.rigid() ||
        !finite(armLength) || armLength < .02 || armLength > 3 || !finite(margin) || margin < 0 ||
        margin > .20)
        return false;
    const V separation = sub(hand.world(grasp.handLocal[0]), object.world(grasp.objectLocal[0]));
    return finite(separation) && norm(separation) <= armLength + margin;
}

// Backward-PD force evaluation from CURRENT native point velocities. This is
// not a local object integrator, teleport, mass edit, constraint replacement or
// release impulse. The native solver owns collisions, hinges and momentum.
inline gx_physics::ForcePacket drive(const gx_physics::Grasp& grasp, const Frame& object, const Frame& hand,
                                     double nativeMass, double simulationDt, bool door, bool natural,
                                     V targetVelocity, unsigned handCount, V center, const V* pointVelocity,
                                     const DriveLimits& limits = DriveLimits())
{
    gx_physics::ForcePacket out;
    if (!grasp.held || !grasp.token.valid() || !grasp.epoch || !object.rigid() || !hand.rigid() ||
        !finite(nativeMass) || nativeMass <= 0 || (door && nativeMass > 200) || !finite(simulationDt) ||
        simulationDt <= 0 || simulationDt > .05 || handCount < 1 || handCount > 2 || !finite(center) ||
        !pointVelocity || !limits.valid() || !finite(targetVelocity) || norm(targetVelocity) > 20)
        return out;
    const double omega = limits.responseRate;
    const double denominator = 1 + 2 * omega * simulationDt + omega * omega * simulationDt * simulationDt;
    const double stiffness = omega * omega / denominator, damping = 2 * omega / denominator;
    // build158 G: doors keep their (working) budget. Anything else (ragdoll parts, other bodies): each hand its own human
    // ceiling x the menu strength, no mass refusal (157: > 20 kg refused), torque = that force x the grasp's lever.
    const double humanForce = gx_human::HAND_N * limits.force / DriveLimits().force;
    const double forceBudget =
        door ? (nativeMass * limits.acceleration < limits.force ? nativeMass * limits.acceleration
                                                                : limits.force) /
                   handCount
             : humanForce;
    const double torqueBudget =
        door ? limits.torque / handCount
             : gx_human::handTorque(humanForce, norm(sub(object.world(grasp.objectLocal[0]), center)));
    const V reference = natural ? limited(targetVelocity, 6) : V();
    for (unsigned i = 0; i < 3; ++i)
    {
        out.point[i] = object.world(grasp.objectLocal[i]);
        const V separation = sub(hand.world(grasp.handLocal[i]), out.point[i]);
        if (!finite(separation) || !finite(pointVelocity[i]))
            return gx_physics::ForcePacket();
        // Resistance saturates the motor, rather than masquerading as lost
        // tracking and dropping the grip. Anatomical escape is checked by the
        // native owner before dispatch; these finite limits remain unchanged.
        const V error = limited(separation, limits.error);
        const V acceleration = sub(mul(error, stiffness), mul(sub(pointVelocity[i], reference), damping));
        out.force[i] = limited(mul(acceleration, nativeMass / 3), forceBudget / 3);
        out.net = add(out.net, out.force[i]);
        out.torque = add(out.torque, gx_hands::cross(sub(out.point[i], center), out.force[i]));
    }
    double scale = 1;
    const double total = norm(out.net), turn = norm(out.torque);
    if (total > forceBudget)
        scale = forceBudget / total;
    if (turn > torqueBudget && torqueBudget / turn < scale)
        scale = torqueBudget / turn;
    if (scale < 1)
    {
        for (unsigned i = 0; i < 3; ++i)
            out.force[i] = mul(out.force[i], scale);
        out.net = mul(out.net, scale);
        out.torque = mul(out.torque, scale);
    }
    out.dt = simulationDt;
    out.valid = finite(out.net) && finite(out.torque);
    return out;
}

// Continuous native contact push. Actual point/normal/body admission remains
// with PhysicsQueries/PhysicsMovable; no force follows unknown occupancy.
inline V pushForce(const gx_physics::Impact& impact, double nativeMass, bool door, unsigned handCount,
                   V point, V center)
{
    if (impact.kind != gx_physics::PUSH || !finite(impact.direction) || norm(impact.direction) < .99 ||
        norm(impact.direction) > 1.01 || !finite(nativeMass) || nativeMass <= 0 ||
        (door && nativeMass > 200) || handCount < 1 || handCount > 2 || !finite(point) || !finite(center))
        return V();
    // build158 G: an open-hand push on a body (not a door): the hand brings the object up to its own speed (the same
    // 10 / s closing response as the body contact), within one hand's human ceiling; no 20 kg refusal, no 8 x mass / 120 N /
    // 30 N*m numbers of ours. Torque is the force times its real lever (native solver).
    if (!door)
    {
        const double closing =
            finite(impact.closingSpeed) && impact.closingSpeed > 0 ? impact.closingSpeed : 0;
        const double wanted = nativeMass * 10 * closing;
        return mul(impact.direction, wanted < gx_human::HAND_N ? wanted : gx_human::HAND_N);
    }
    double budget = (nativeMass * 8 < 120 ? nativeMass * 8 : 120) / handCount;
    const double lever = norm(gx_hands::cross(sub(point, center), impact.direction));
    const double torqueBudget = 30.0 / handCount;
    if (lever > 0 && budget * lever > torqueBudget)
        budget = torqueBudget / lever;
    return mul(impact.direction, budget);
}
}
