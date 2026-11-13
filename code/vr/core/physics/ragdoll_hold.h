#pragma once
// Build160 F (worn 159: corpse grab "pendia de un hilo o habia una fuerza que no estaba pegada a mi mano"; user: "no se si
// estuviste intentando rehacer desde cero sin fijarte como funciona el agarre en los demas objetos"). Codex ragdollhold160
// (CONFIRMED): objects use the gx_physical138 velocity-target motor (gravity compensation, feedforward, drawn hand follows the
// object) while a ragdoll part fell through to the softer gx_tracked_physics three-point spring (k 144 at 60 Hz, no gravity
// term, drawn hand left at the tracked pose). A held ragdoll link now gets the SAME motor translation at the grabbed point:
// velocity target with the desired-motion feedforward, gravity compensation with the link's own native mass, the same human
// ceiling per hand (251 N at 100%, hands add). The link hangs from that point (a limb held at one point); no whole-body mass,
// kinematic attach or teleport. No game headers, C++03.
#include "physical_math.h"
namespace gx_ragdoll
{
using namespace gx_physics;
struct Packet
{
    bool valid;
    V point, force;
    double positionError;
    Packet() : valid(false), positionError(0) {}
};
// Codex reviewFG160 F1 (CONFIRMED): the link mass is not the mass of a point off the link centre (a limb end responds up to 4x
// faster), so the one-step velocity correction injected energy at 60-90 Hz. The correction now uses the point's native
 
 
// fall. A point with no mobility (fixed root, invalid matrix) gets no correction, only the weight term.
inline double pointMass(const V invMass[3], V d)
{
    const V response = add(add(mul(invMass[0], d.x), mul(invMass[1], d.y)), mul(invMass[2], d.z));
    const double s = dot(d, response);
    return finite(s) && s > 0 ? 1 / s : 0;
}
inline Packet drive(const Grasp& g, const Frame& body, const Frame& hand, V pointVelocity, V gravity,
                    const V invMass[3], double linkMass, double dt, unsigned holders, V linearReference,
                    const gx_physical::Limits& limits)
{
    Packet out;
    if (!invMass || !finite(invMass[0]) || !finite(invMass[1]) || !finite(invMass[2]))
        return out;
    if (!g.held || !g.token.valid() || !g.epoch || !body.rigid() || !hand.rigid() || !finite(pointVelocity) ||
        !finite(gravity) || !finite(linearReference) || norm(linearReference) > gx_motion::GLITCH ||
        !finite(linkMass) || linkMass <= 0 || !finite(dt) || dt <= 0 || dt > .05 || holders < 1 ||
        holders > 2 || !limits.valid())
        return out;
    out.point = body.world(g.objectLocal[0]);
    const V target = hand.world(g.handLocal[0]);
    // the object motor's time constant: never below one step (deadbeat, no overshoot)
    double tau = 1 / limits.responseRate;
    if (tau < dt)
        tau = dt;
    const V rawError = sub(target, out.point);
    out.positionError = norm(rawError);
    const V want = add(linearReference, mul(limited(rawError, limits.error), 1 / tau));
    // the link's own weight, then the velocity correction with the point's effective mass
    V force = mul(gravity, -linkMass);
    const V velocityError = sub(want, pointVelocity);
    const double speed = norm(velocityError);
    if (speed > 0)
    {
        const V d = mul(velocityError, 1 / speed);
        force = add(force, mul(d, pointMass(invMass, d) * speed / dt));
    }
    // each hand on the SAME link its share of the demand and its own human ceiling (build158 G, reviewFG160 F2)
    out.force = limited(mul(force, 1.0 / holders), limits.force);
    out.valid = finite(out.force) && finite(out.point);
    return out;
}
// build161 F (worn 160: the held part "se empieza a mover hacia lados que yo no estoy tironeando", the drawn hand turning and
 
// angular response of that link (phArticulatedCollider::GetInverseInertiaMatrixArt, the matrix it applies an angular impulse
// with) is projected on the angular velocity error, as F1 does for the point, so the step is exact on that axis and the error
// energy only falls. Ceiling: the hand's torque at the grabbed point (gx_human158::handTorque), shared by the hands on the link.
struct Turn
{
    bool valid;
    V angularImpulse;
    double angleError;
    Turn() : valid(false), angleError(0) {}
};
inline Turn turn(const Grasp& g, const Frame& body, const Frame& hand, V spin, V angularReference,
                 const V invInertia[3], V linkCentre, double dt, unsigned holders,
                 const gx_physical::Limits& limits)
{
    Turn out;
    if (!invInertia || !finite(invInertia[0]) || !finite(invInertia[1]) || !finite(invInertia[2]) ||
        !finite(spin) || !finite(angularReference) || !finite(linkCentre) || !g.held || !g.token.valid() ||
        !g.epoch || !body.rigid() || !hand.rigid() || !finite(dt) || dt <= 0 || dt > .05 || holders < 1 ||
        holders > 2 || !limits.valid())
        return out;
    const Frame target = gx_rigid_grasp::desired(g, hand);
    if (!target.rigid())
        return out;
    double tau = 1 / limits.responseRate;
    if (tau < dt)
        tau = dt;
    const V rotation = gx_rigid_grasp::rotationError(body, target);
    out.angleError = norm(rotation);
    const V error = sub(add(angularReference, mul(rotation, 1 / tau)), spin);
    const double speed = norm(error);
    if (speed > 0)
    {
        const V axis = mul(error, 1 / speed);
        const double lever = norm(sub(body.world(g.objectLocal[0]), linkCentre));
        out.angularImpulse = limited(mul(axis, pointMass(invInertia, axis) * speed / holders),
                                     gx_human::handTorque(limits.force, lever) * dt);
    }
    out.valid = finite(out.angularImpulse);
    return out;
}
}
