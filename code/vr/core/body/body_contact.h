#pragma once
// GTAVXR-authored bounded contact response, not a second physics simulation.
 
#include "../physics/tracked_physics.h"
// build158 G: human strength (sourced)
#include "../physics/human_strength.h"

namespace gx_body_contact
{
using gx_physics::V;
using gx_physics::Frame;
using gx_physics::Token;
using gx_physics::finite;
using gx_physics::norm;
using gx_physics::add;
using gx_physics::sub;
using gx_physics::mul;
struct Sample
{
    unsigned identity, activation, mode;
    uint64_t frame, generation, context;
    uintptr_t shape, director;
    Frame pose;
    V root;
    double time, dt;
    Sample()
        : identity(0), activation(0), mode(0), frame(0), generation(0), context(0), shape(0), director(0),
          time(0), dt(0)
    {
    }
    bool valid() const
    {
        return identity && activation && context && frame && generation && shape && pose.rigid() &&
               finite(root) && finite(time) && finite(dt) && dt > 0 && dt <= .05;
    }
};
// Two actual published transforms, never native last-matrix animation mixed
// with a current VR pose. Model/LOD/recenter/teleport/invalid time reseed only
// this force witness, not the HMD, controller or VRIK tracking state.
class History
{
    Sample previous_, current_;
    bool seeded_, ready_;

  public:
    History() : seeded_(false), ready_(false) {}
    void reset()
    {
        *this = History();
    }
    bool update(const Sample& s)
    {
        if (!s.valid())
        {
            reset();
            return false;
        }
        if (seeded_ && s.identity == current_.identity && s.frame == current_.frame &&
            s.generation == current_.generation)
            return false;
        const double elapsed = s.time - current_.time;
        ready_ = seeded_ && s.identity == current_.identity && s.activation == current_.activation &&
                 s.context == current_.context && s.mode == current_.mode &&
                 s.director == current_.director && s.shape == current_.shape &&
                 s.frame == current_.frame + 1 && s.generation == current_.generation + 1 &&
                 elapsed >= .001 && elapsed <= .10 && norm(sub(s.root, current_.root)) <= .35 &&
                 norm(sub(s.pose.p, current_.pose.p)) <= gx_motion::GLITCH * elapsed;
        previous_ = current_;
        current_ = s;
        seeded_ = true;
        return ready_;
    }
    bool velocity(V point, V& out) const
    {
        out = V();
        if (!ready_ || !finite(point))
            return false;
        const V old = previous_.pose.world(current_.pose.local(point));
        const V change = sub(point, old);
        const double realDt = current_.time - previous_.time;
        if (!finite(change) || norm(change) / realDt > gx_motion::GLITCH)
            return false;
        // The glitch filter and published velocity share real seconds; slow motion cannot reintroduce a speed ceiling.
        out = mul(change, 1 / realDt);
        return finite(out);
    }
};
 
// Keep our classification names distinct without undefining native semantics.
enum Target
{
    RIGID_PROP,
    WORLD_HINGE,
    ARTICULATED_DOOR_TARGET
};
struct Packet
{
    Token token;
    V point, center, force;
    double mass;
    Target kind;
    bool valid;
    Packet() : mass(0), kind(RIGID_PROP), valid(false) {}
};
inline Packet contact(Token token, V point, V normal, double depth, V velocity, V objectVelocity, double mass,
                      V center, Target kind)
{
    Packet p;
    if (!token.valid() || !finite(point) || !finite(center) || !finite(normal) || norm(normal) < .99 ||
        norm(normal) > 1.01 || !finite(depth) || depth < 0 || depth > .15 || !finite(velocity) ||
        norm(velocity) > gx_motion::GLITCH || !finite(objectVelocity) || kind < RIGID_PROP ||
        kind > ARTICULATED_DOOR_TARGET || !finite(mass) || mass <= 0)
        return p;
    // Native phIntersection normal points OUT of the intersected target.
    // Only compressive pressure is admitted; no pull, damage or release boost.
    const V inward = mul(normal, -1);
    const double approaching = gx_hands::dot(sub(velocity, objectVelocity), inward);
    if (!finite(approaching) || approaching < -.05)
        return p;
    const double speed = approaching > 0 ? approaching : 0;
    const double acceleration = 10 * speed + 40 * depth;
    // build158 G: the body leans with a person's standing push (gx_human158::BODY_N), not 120 N or 8 m/s2 x mass of ours;
    // the 10 / s closing + 40 / s2 depth response already brings a light object only up to the body's speed.
    const double budget = gx_human::BODY_N;
    const double desired = mass * acceleration;
    const double force = desired < budget ? desired : budget;
    if (!finite(force) || force <= 0)
        return p;
    p.token = token;
    p.point = point;
    p.center = center;
    p.force = mul(inward, force);
    p.mass = mass;
    p.kind = kind;
    p.valid = true;
    return p;
}
// Sum ABSOLUTE force/torque contributions, not only net vectors: opposing
// contacts cannot cancel the accounting and amplify root/hinge response.
// Shares one finite budget per native instance, plus one whole-body budget.
inline bool budget(Packet* p, unsigned count)
{
    if (!p || !count || count > 64)
        return false;
    double scales[64];
    double total = 0;
    for (unsigned i = 0; i < count; ++i)
    {
        if (!p[i].valid || !p[i].token.valid() || !finite(p[i].force) || !finite(p[i].point) ||
            !finite(p[i].center) || !finite(p[i].mass) || p[i].mass <= 0 || p[i].kind < RIGID_PROP ||
            p[i].kind > ARTICULATED_DOOR_TARGET)
            return false;
        double force = 0, torque = 0, limit = gx_human::BODY_N;
        for (unsigned j = 0; j < count; ++j)
            if (p[i].token.sameBody(p[j].token))
            {
                if (!p[j].valid || !finite(p[j].point) || !finite(p[j].center) || !finite(p[j].force) ||
                    !finite(p[j].mass) || p[j].mass <= 0)
                    return false;
                force += norm(p[j].force);
                torque += norm(gx_hands::cross(sub(p[j].point, p[j].center), p[j].force));
            }
        double scale = force > limit ? limit / force : 1;
        (void)torque;
        if (!finite(scale) || scale < 0 || scale > 1)
            return false;
        scales[i] = scale;
        total += norm(p[i].force) * scale;
    }
    // the whole body has one person's push in total, shared by every touched body
    if (!finite(total))
        return false;
    const double global = total > gx_human::BODY_N ? gx_human::BODY_N / total : 1;
    for (unsigned i = 0; i < count; ++i)
        p[i].force = mul(p[i].force, scales[i] * global);
    return true;
}
}
