#pragma once
// Admission-only motion. Never a motor, tracking writer, query or damage system.
#include "physical_strike.h"
namespace gx_hand_motion
{
using gx_physics::V;
using gx_physics::Frame;
inline V rotate(const Frame& f, V v)
{
    return gx_physics::sub(f.world(v), f.p);
}
// Exact row-vector quaternion formula used by pose075::pose. No axis guess.
inline bool rawPose(double px, double py, double pz, double x, double y, double z, double w, Frame& out)
{
    if (!gx_physics::finite(V(px, py, pz)) || !gx_physics::finite(x) || !gx_physics::finite(y) ||
        !gx_physics::finite(z) || !gx_physics::finite(w) || fabs(x * x + y * y + z * z + w * w - 1) >= 1e-5)
        return false;
    out.a = V(1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w));
    out.b = V(2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w));
    out.c = V(2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y));
    out.p = V(px, py, pz);
    return out.rigid();
}
inline bool worldFrame(const double* m, Frame& out)
{
    if (!m)
        return false;
    for (unsigned i = 0; i < 16; ++i)
        if (!gx_physics::finite(m[i]))
            return false;
    if (fabs(m[3]) > 1e-8 || fabs(m[7]) > 1e-8 || fabs(m[11]) > 1e-8 || fabs(m[15] - 1) > 1e-8)
        return false;
    out.a = V(m[0], m[1], m[2]);
    out.b = V(m[4], m[5], m[6]);
    out.c = V(m[8], m[9], m[10]);
    out.p = V(m[12], m[13], m[14]);
    return out.rigid();
}
// hand_world has W=Q*M. Recover precisely M=inverse(Q)*W from SAME raw
// local pose and BEFORE-calibration world pose, not the native ped transform.
inline bool mapping(const Frame& q, const Frame& rawWorld, Frame& out)
{
    if (!q.rigid() || !rawWorld.rigid())
        return false;
    // Pose accepts small finite quaternion norm error. Use the actual inverse,
    // not an assumed-exact transpose, so those raw Q bytes cancel faithfully.
    const V aa = gx_hands::cross(q.b, q.c), bb = gx_hands::cross(q.c, q.a), cc = gx_hands::cross(q.a, q.b);
    const double d = gx_physics::dot(q.a, aa);
    if (!gx_physics::finite(d) || d <= .99)
        return false;
    out.a = gx_physics::mul(gx_physics::add(gx_physics::mul(rawWorld.a, aa.x),
                                            gx_physics::add(gx_physics::mul(rawWorld.b, bb.x),
                                                            gx_physics::mul(rawWorld.c, cc.x))),
                            1 / d);
    out.b = gx_physics::mul(gx_physics::add(gx_physics::mul(rawWorld.a, aa.y),
                                            gx_physics::add(gx_physics::mul(rawWorld.b, bb.y),
                                                            gx_physics::mul(rawWorld.c, cc.y))),
                            1 / d);
    out.c = gx_physics::mul(gx_physics::add(gx_physics::mul(rawWorld.a, aa.z),
                                            gx_physics::add(gx_physics::mul(rawWorld.b, bb.z),
                                                            gx_physics::mul(rawWorld.c, cc.z))),
                            1 / d);
    out.p = gx_physics::sub(rawWorld.p, rotate(out, q.p));
    return out.rigid();
}
inline bool sameMapping(const Frame& a, const Frame& b, double tolerance = 1e-4)
{
    return a.rigid() && b.rigid() && gx_physics::norm(gx_physics::sub(a.a, b.a)) <= tolerance &&
           gx_physics::norm(gx_physics::sub(a.b, b.b)) <= tolerance &&
           gx_physics::norm(gx_physics::sub(a.c, b.c)) <= tolerance &&
           gx_physics::norm(gx_physics::sub(a.p, b.p)) <= tolerance;
}
struct Stamp
{
    bool valid;
    unsigned epoch, activation, nativeFrame, mode;
    uintptr_t director;
    uint64_t frame, generation, sequence, context, serial, tick, inputTick;
    int64_t displayTime, inputDisplayTime;
    double time; // time: QPC seconds for dt (build147 A)
    Stamp()
        : valid(false), epoch(0), activation(0), nativeFrame(0), mode(0), director(0), frame(0),
          generation(0), sequence(0), context(0), serial(0), tick(0), inputTick(0), displayTime(0),
          inputDisplayTime(0), time(0)
    {
    }
    bool usable() const
    {
        return valid && epoch && activation && nativeFrame && frame && generation && sequence && context &&
               serial && tick && inputTick && displayTime > 0 && inputDisplayTime > 0;
    }
    bool same(const Stamp& s) const
    {
        return usable() && s.usable() && epoch == s.epoch && activation == s.activation &&
               nativeFrame == s.nativeFrame && mode == s.mode && director == s.director && frame == s.frame &&
               generation == s.generation && sequence == s.sequence && context == s.context &&
               serial == s.serial && tick == s.tick && inputTick == s.inputTick &&
               displayTime == s.displayTime && inputDisplayTime == s.inputDisplayTime;
    }
};
struct Hand
{
    bool valid;
    Frame raw, map;
    V desired;
    int mm[3], pitch;
    Hand() : valid(false), pitch(0)
    {
        mm[0] = mm[1] = mm[2] = 0;
    }
    bool usable() const
    {
        return valid && raw.rigid() && map.rigid() && gx_physics::finite(desired) && pitch >= -50 &&
               pitch <= 50 && mm[0] >= -150 && mm[0] <= 150 && mm[1] >= -150 && mm[1] <= 150 &&
               mm[2] >= -150 && mm[2] <= 150;
    }
    bool sameCalibration(const Hand& h) const
    {
        return pitch == h.pitch && mm[0] == h.mm[0] && mm[1] == h.mm[1] && mm[2] == h.mm[2];
    }
};
struct Publication
{
    Stamp stamp;
    Hand hand[2];
    unsigned mask;
    Publication() : mask(0) {}
};
inline bool consecutive(const Stamp& a, const Stamp& b)
{
    return a.usable() && b.usable() && b.epoch == a.epoch && b.activation == a.activation &&
           b.context == a.context && b.mode == a.mode && b.director == a.director &&
           b.nativeFrame == a.nativeFrame + 1 && b.frame == a.frame + 1 && b.generation == a.generation + 1 &&
           b.serial == a.serial + 1 && b.sequence >= a.sequence && b.inputDisplayTime >= a.inputDisplayTime &&
           b.inputTick >= a.inputTick && b.displayTime > a.displayTime;
}
struct Velocity
{
    bool valid;
    V own;
    Velocity() : valid(false) {}
};
class History
{
    bool ready_;
    Stamp previousStamp_;
    Hand previous_;

  public:
    History() : ready_(false) {}
    void unknown()
    {
        ready_ = false;
        previousStamp_ = Stamp();
        previous_ = Hand();
    }
    Velocity update(const Stamp& s, const Hand& h, bool actualCurrentQueryAndOwner)
    {
        Velocity out;
        if (!actualCurrentQueryAndOwner || !s.usable() || !h.usable())
        {
            unknown();
            return out;
        }
        const double dt = ready_ && s.time > previousStamp_.time ? s.time - previousStamp_.time : 0;
        // Native UPDATE/generation and XR frame advance once here. Any skipped,
        // replayed, wrong-camera or reordered cohort seeds, never makes a blow.
        const bool continuous =
            ready_ && consecutive(previousStamp_, s) && dt >= .001 && dt <= .1 &&
            h.sameCalibration(previous_) &&
            gx_physics::norm(gx_physics::sub(h.raw.p, previous_.raw.p)) <= gx_motion::GLITCH * dt;
        if (continuous)
        {
            // Transport the previous CALIBRATED desired palm via the actual maps.
            // Raw wrist rotation moves its local offset; reference-only rigid
            // translation/yaw does not. Solved/stopped contact is never sampled.
            const V transported = h.map.world(previous_.map.local(previous_.desired));
            out.own = gx_physics::mul(gx_physics::sub(h.desired, transported), 1 / dt);
            out.valid = gx_physics::finite(out.own) && gx_physics::norm(out.own) <= gx_motion::GLITCH;
        }
        previousStamp_ = s;
        previous_ = h;
        ready_ = true;
        return out;
    }
};
}
