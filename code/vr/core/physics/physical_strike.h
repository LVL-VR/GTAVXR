#pragma once
// Independently authored admission/episode values, NOT another damage system.
// A qualified native producer supplies contact/currentness; this follows no pointer.
#include "physics_contact.h"
namespace gx_strike
{
using gx_physics::V;
using gx_physics::Token;
using gx_physics::Impact;
using gx_physics::Hit;
inline bool fistIntent(bool unheld, bool eligible, double grip, double trigger)
{
    return unheld && eligible && gx_physics::finite(grip) && gx_physics::finite(trigger) && grip >= .65 &&
           grip <= 1 && trigger >= .65 && trigger <= 1;
}
inline bool npcHandMovement(V velocity, V normal)
{
    if (!gx_physics::finite(velocity) || !gx_physics::finite(normal))
        return false;
    const double speed = gx_physics::norm(velocity), n = gx_physics::norm(normal);
    return speed >= .25 && speed <= gx_motion::GLITCH && n > .99 && n < 1.01 &&
           -gx_physics::dot(velocity, normal) >= .25;
}
inline Impact ownMotion(const Hit& hit, V own, V target, bool deliberate)
{
    Impact out = gx_physics::classify(hit, own, target, deliberate);
    if (!npcHandMovement(own, hit.normal))
        return Impact();
    const double speed = gx_physics::norm(own), closing = -gx_physics::dot(own, hit.normal);
    if (out.kind == gx_physics::INTENTIONAL_STRIKE &&
        (closing < gx_strike_curve::gate(gx_strike_curve::FIST) || closing / speed < .55))
        out.kind = gx_physics::PUSH;
    out.direction = gx_physics::mul(own, 1 / speed); // actual hand blow, not NPC motion
    return out;
}
class Episode
{
    Token spent_;
    unsigned epoch_;
    uint64_t phase_;
    double last_;
    unsigned clear_;
    bool armed_;
    bool sample(unsigned epoch, uint64_t phase, double time)
    {
        if (!epoch || !phase || !gx_physics::finite(time) || time < 0)
        {
            unknown();
            return false;
        }
        if (epoch_ != epoch)
        {
            epoch_ = epoch;
            phase_ = 0;
            clear_ = 0;
            armed_ = false;
            spent_ = Token();
            last_ = -1;
        }
        if (phase <= phase_)
        {
            return false;
        }
        phase_ = phase;
        return true;
    }

  public:
    Episode() : epoch_(0), phase_(0), last_(-1), clear_(0), armed_(false) {}
    // Query/identity/intent loss never proves separation or clears spent body.
    void unknown()
    {
        clear_ = 0;
        armed_ = false;
    }
    // A native runtime boundary revokes admission, but is not separation. XR
    // frames can restart within the same actor epoch: keep only its body/cooldown
    // fence and release the old phase. A new actor epoch is purged by sample().
    void boundary()
    {
        unknown();
        phase_ = 0;
    }
    bool clear(unsigned epoch, uint64_t phase, double time, bool completeFreeEndpoint)
    {
        if (!sample(epoch, phase, time))
            return false;
        if (!completeFreeEndpoint)
        {
            unknown();
            return false;
        }
        if (clear_ < 2)
            ++clear_;
        armed_ = clear_ >= 2;
        return armed_;
    }
    bool contact(Token target, unsigned epoch, uint64_t phase, double time, gx_physics::ContactKind kind)
    {
        if (!sample(epoch, phase, time))
            return false;
        if (!target.valid())
        {
            unknown();
            return false;
        }
        // Every real contact ends a stroke. Same-body component changes, query
        // holes and input release cannot start another stroke while overlapping.
        const bool fire =
            armed_ && kind == gx_physics::INTENTIONAL_STRIKE && (last_ < 0 || time - last_ >= .25);
        armed_ = false;
        clear_ = 0;
        if (fire)
        {
            spent_ = target;
            last_ = time;
        } // consume BEFORE native event, even zero damage
        return fire;
    }
    Token spent() const
    {
        return spent_;
    }
};
struct OwnerWindow
{
    uintptr_t actor;
    unsigned epoch, frame;
    bool native;
    OwnerWindow() : actor(0), epoch(0), frame(0), native(false) {}
    void observe(uintptr_t actualActor, unsigned actualEpoch, unsigned nativeFrame)
    {
        if (actualActor && actualEpoch && nativeFrame)
        {
            actor = actualActor;
            epoch = actualEpoch;
            frame = nativeFrame;
            native = true;
        }
    }
    bool owned(uintptr_t actualActor, unsigned actualEpoch, unsigned nativeFrame) const
    {
        return native && actor == actualActor && epoch == actualEpoch && frame == nativeFrame;
    }
};
inline V point(const float* m, V p)
{
    return V(m[0] * p.x + m[3] * p.y + m[6] * p.z + m[9], m[1] * p.x + m[4] * p.y + m[7] * p.z + m[10],
             m[2] * p.x + m[5] * p.y + m[8] * p.z + m[11]);
}
inline V local(const float* m, V p)
{
    p = gx_physics::sub(p, V(m[9], m[10], m[11]));
    return V(m[0] * p.x + m[1] * p.y + m[2] * p.z, m[3] * p.x + m[4] * p.y + m[5] * p.z,
             m[6] * p.x + m[7] * p.y + m[8] * p.z);
}
struct PointMotion
{
    bool valid;
    V own, world, target;
    PointMotion() : valid(false) {}
};
inline bool rigid(const float* m)
{
    if (!m)
        return false;
    gx_physics::Frame f;
    f.a = V(m[0], m[1], m[2]);
    f.b = V(m[3], m[4], m[5]);
    f.c = V(m[6], m[7], m[8]);
    f.p = V(m[9], m[10], m[11]);
    return f.rigid();
}
// All matrices are current/previous actual rigid native matrices, checked by
// producer. Transport the same contact point: rotations count, root motion doesn't.
inline PointMotion motion(const float* oldLeaf, const float* leaf, const float* oldRoot, const float* root,
                          const float* oldChild, const float* child, V contact, double dt)
{
    PointMotion out;
    if (!rigid(oldLeaf) || !rigid(leaf) || !rigid(oldRoot) || !rigid(root) || !rigid(oldChild) ||
        !rigid(child) || !gx_physics::finite(contact) || !gx_physics::finite(dt) || dt < .001 || dt > .1)
        return out;
    const V oldPoint = point(oldLeaf, local(leaf, contact));
    out.world = gx_physics::mul(gx_physics::sub(contact, oldPoint), 1 / dt);
    const V transported = point(root, local(oldRoot, oldPoint));
    out.own = gx_physics::mul(gx_physics::sub(contact, transported), 1 / dt);
    out.target = gx_physics::mul(gx_physics::sub(contact, point(oldChild, local(child, contact))), 1 / dt);
    out.valid = gx_physics::finite(out.own) && gx_physics::finite(out.world) &&
                gx_physics::finite(out.target) && gx_physics::norm(out.own) <= gx_motion::GLITCH &&
                gx_physics::norm(out.world) <= gx_motion::GLITCH &&
                gx_physics::norm(out.target) <= gx_motion::GLITCH;
    return out;
}
}
