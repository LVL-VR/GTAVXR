#pragma once
// Build141 B1/B2 CONTRACT (frozen by Claude before delegation; implementation lives in strike_math.h).
// PLANCK-like melee core: pure math/policy, VS2012-compatible, NO game/runtime state, NO PLANCK/HIGGS code
// (behavioural reference only). Units: metres, seconds, m/s, rad/s; world space = the engine world space the
 
#include "physics_contact.h"
namespace gx_strike_math
{
using gx_physics::V;

// ---------------------------------------------------------------- B1 weapon collision proxy
enum WeaponKind
{
    FIST = 0,
    BLUNT = 1,
    BLADE = 2
};
// Grip->tip capsule in the HELD WEAPON OBJECT's model space (same space as its drawable bounds).
struct WeaponProxy
{
    V grip, tip;
    double radius;
    unsigned kind;
    WeaponProxy() : radius(0), kind(BLUNT) {}
    bool valid() const; // finite, |tip-grip| in [0.05,1.6] m, radius in [0.01,0.08] m, kind <= BLADE
};
// Derive the proxy from the weapon model's AABB and the grip point in model space (where the hand holds it):
// axis = longest AABB extent; tip = the AABB end on that axis farthest from the grip; grip projected onto the axis;
// radius = clamp(half of the second-largest extent, 0.012, 0.06). Returns false for degenerate/implausible bounds.
inline bool proxyFromBounds(V boundsMin, V boundsMax, V gripModel, unsigned kind, WeaponProxy& out);

// Rigid world transform of a model-space point (row axes a,b,c + origin p, like gx_physics::Frame).
struct Pose
{
    V p, a, b, c;
};
inline V transformPoint(const Pose& pose, V local);

// ---------------------------------------------------------------- B1 swept contact
// Target body part approximated by a capsule (sphere when a==b) in world space (ragdoll component bound).
struct Capsule
{
    V a, b;
    double r;
    Capsule() : r(0) {}
};
struct SweepHit
{
    bool hit;
    double t; // t in [0,1] along previous->current weapon pose
    V point, normal;
    double depth; // point on the target surface; normal from target towards the weapon
    SweepHit() : hit(false), t(1), depth(0) {}
};
// Conservative swept test of the weapon capsule from its previous to its current world pose (both endpoints move
// linearly in [0,1]) against one target capsule. Must not tunnel through a 0.05 m-radius target for weapon tip
// speeds up to 20 m/s at 90 Hz (substep or analytic). First contact time wins.
inline bool sweep(V gripBefore, V tipBefore, V gripAfter, V tipAfter, double radius, const Capsule& target,
                  SweepHit& out);

// ---------------------------------------------------------------- B1 point velocity
// v(p) = v_center + omega x (p - center)
inline V pointVelocity(V linear, V angular, V center, V point);
// Finite-difference velocity of a body-fixed point between two rigid poses dt apart (dt in [0.001,0.1]).
inline bool pointVelocityFromPoses(const Pose& before, const Pose& after, V local, double dt, V& out);

// ---------------------------------------------------------------- B2 classifier
enum StrikeKind
{
    NO_STRIKE = 0,
    SWING = 1,
    STAB = 2,
    PUNCH = 3
};
struct ClassifierTuning
{
    double swing, stab, punch, minHandSpeed, stabAxisCos, minClosing;
    ClassifierTuning()
        : swing(5.0), stab(2.0), punch(2.5), minHandSpeed(1.0), stabAxisCos(.70), minClosing(.15)
    {
    }
};
struct Classification
{
    StrikeKind kind;
    double speed, closing, intensity;
    V direction; // intensity in [0,1] above the kind's threshold
    Classification() : kind(NO_STRIKE), speed(0), closing(0), intensity(0) {}
};
// relative = weapon contact point velocity - target point velocity; axis = unit grip->tip (ignored for FIST);
// normal = SweepHit::normal; realHandSpeed = TRUE tracked controller linear speed (anti joystick/turning false hits).
// FIST: PUNCH when closing >= punch. BLADE/BLUNT: STAB when the motion is axial (dot(unit(relative),axis) >= stabAxisCos)
// and speed >= stab (BLUNT stab threshold = 1.5 x stab); otherwise SWING when speed >= swing. Always NO_STRIKE when
// realHandSpeed < minHandSpeed or closing < minClosing or any input is non-finite.
inline Classification classify(unsigned weaponKind, V relative, V axis, V normal, double realHandSpeed,
                               const ClassifierTuning& tuning);

// ---------------------------------------------------------------- B2 per-hand/per-target gate
struct GateTuning
{
    double cooldown, separation;
    GateTuning() : cooldown(.25), separation(.12) {}
};
// One admitted hit per contact EPISODE per (hand,target). An episode ends only after the target has been out of
// contact continuously for >= separation seconds; a new hit also needs >= cooldown since that hand's last hit on that
// target. Weapon resting inside a body never repeats damage. Bounded slots (no allocation); oldest slot recycled.
class HitGate
{
  public:
    enum
    {
        SLOTS = 16
    };
    HitGate();
    void reset();
    // Called every update for every (hand,target) the hand currently touches or touched last update.
    // Returns true exactly when a strike classified this update may be dispatched.
    bool admit(unsigned hand, uint64_t target, double now, bool contact, bool strike,
               const GateTuning& tuning);
    // Hand lost tracking/weapon changed/actor epoch changed: forget that hand's episodes.
    void forgetHand(unsigned hand);

  private:
    struct Slot
    {
        uint64_t target;
        unsigned hand;
        bool used, inside;
        double lastContact, lastHit;
    };
    Slot slots_[SLOTS];
};

// ---------------------------------------------------------------- B3 response plan (engine glue consumes)
enum Response
{
    RESPONSE_NONE = 0,
    FLINCH = 1,
    STAGGER = 2,
    RAGDOLL = 3
};
struct ResponseTuning
{
    double stagger, ragdoll, impulseMin, impulseMax;
    ResponseTuning()
        : stagger(.45), ragdoll(.85), impulseMin(4), impulseMax(12)
    {} // impulse in native sPhysicalMeleeInfo units
};
struct ResponsePlan
{
    Response kind;
    double impulse;
    V direction;
    ResponsePlan() : kind(RESPONSE_NONE), impulse(0) {}
};
// power = trigger held (PLANCK power attack) -> RAGDOLL. Else intensity < stagger -> FLINCH, < ragdoll -> STAGGER,
// else RAGDOLL. impulse = lerp(impulseMin,impulseMax,intensity) * massFactor (massFactor clamped [0.5,1.5]),
// clamped to impulseMax; direction = unit(classification.direction). NO_STRIKE -> RESPONSE_NONE.
inline ResponsePlan respond(const Classification& c, bool power, double massFactor,
                            const ResponseTuning& tuning);
}
