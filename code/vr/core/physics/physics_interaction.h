#pragma once
// GTAVXR-owned decisions around ONE native simulation. No object integration.
#include "physics_contact.h"

namespace gx_interaction
{
using gx_physics::V;
using gx_physics::Frame;
using gx_physics::Grasp;
using gx_physics::Token;

struct DoorState
{
    bool enabled, standard, worldJoint, liveJoint, scriptOwned, networkOwned, codeControlled, cutscene;
    bool fixed, attached, kinematic, stuck, automatic, holdOpen, hasData, unlocked, pending, removing,
        flushing;
    double mass;
    DoorState()
        : enabled(false), standard(false), worldJoint(false), liveJoint(false), scriptOwned(false),
          networkOwned(false), codeControlled(false), cutscene(false), fixed(false), attached(false),
          kinematic(false), stuck(false), automatic(false), holdOpen(false), hasData(false), unlocked(false),
          pending(false), removing(false), flushing(false), mass(0)
    {
    }
};
inline bool physicalDoor(const DoorState& s)
{
    return s.enabled && s.standard && s.worldJoint && s.liveJoint && !s.scriptOwned && !s.networkOwned &&
           !s.codeControlled && !s.cutscene && !s.fixed && !s.attached && !s.kinematic && !s.stuck &&
           !s.automatic && !s.holdOpen &&
           (!s.hasData || (s.unlocked && !s.pending && !s.removing && !s.flushing)) &&
           gx_physics::finite(s.mass) && s.mass > 0 && s.mass <= 200;
}

struct Geometry
{
    uint64_t instance, whole, part;
    unsigned lod;
    Geometry() : instance(0), whole(0), part(0), lod(0) {}
    bool valid() const
    {
        return instance && whole && part;
    }
    bool same(const Geometry& g) const
    {
        return valid() && g.valid() && instance == g.instance && whole == g.whole && part == g.part &&
               lod == g.lod;
    }
};
inline bool rigidFragment(bool sameInstance, bool cached, bool initialized, bool hasType, bool door,
                          bool articulatedProp, bool authoredBody, bool liveBody)
{
    return sameInstance && cached && initialized && hasType &&
           (door || (!articulatedProp && !authoredBody && !liveBody));
}
// A previous grip is not a current collision exclusion. Native admission,
// exact geometry, real ownership and tracking must all still hold this frame.
inline Token heldContactExclusion(const Grasp& grasp, bool currentBody, bool eligible, bool tracked)
{
    return grasp.held && currentBody && eligible && tracked ? grasp.token : Token();
}

// Every admission is evaluated before committing either hand. A released hand
// never detaches or modifies the native object; transfer keeps native momentum.
struct PairPlan
{
    Grasp next[2];
    unsigned acquired, released, transfer;
    PairPlan() : acquired(0), released(0), transfer(0) {}
};
inline PairPlan plan(const Grasp* before, const bool* keep, const Grasp* prospective, const bool* acquire,
                     const bool* neutralRelease)
{
    PairPlan out;
    for (unsigned h = 0; h < 2; ++h)
    {
        if (before[h].held && keep[h])
            out.next[h] = before[h];
        else if (acquire[h] && prospective[h].held)
        {
            out.next[h] = prospective[h];
            out.acquired |= 1u << h;
        }
        if (before[h].held && !keep[h])
            out.released |= 1u << h;
    }
    for (unsigned h = 0; h < 2; ++h)
        if ((out.released & (1u << h)) && neutralRelease[h] && out.next[1 - h].held &&
            before[h].epoch == out.next[1 - h].epoch && before[h].token.sameBody(out.next[1 - h].token))
            out.transfer |= 1u << h;
    return out;
}
inline unsigned holders(const Grasp* hands, Token token)
{
    unsigned n = 0;
    for (unsigned h = 0; h < 2; ++h)
        if (hands[h].held && hands[h].token.sameBody(token))
            ++n;
    return n;
}

enum ReleaseKind
{
    NO_RELEASE,
    NATIVE_RELEASE,
    NATIVE_THROW,
    HAND_TRANSFER,
    SAFETY_RELEASE,
    NATIVE_RECOVERY
};
struct ReleaseReceipt
{
    ReleaseKind kind;
    Token target;
    unsigned epoch;
    uint64_t frame, generation;
    V nativeVelocity, roomVelocity;
    bool measured;
    ReleaseReceipt() : kind(NO_RELEASE), epoch(0), frame(0), generation(0), measured(false) {}
};
inline ReleaseKind releaseKind(bool deliberate, bool transferred, bool nativeRecovery, bool measured,
                               V nativeVelocity, V roomVelocity)
{
    if (transferred && deliberate)
        return HAND_TRANSFER;
    if (nativeRecovery)
        return NATIVE_RECOVERY;
    if (!deliberate)
        return SAFETY_RELEASE;
    if (measured && gx_physics::finite(nativeVelocity) && gx_physics::finite(roomVelocity))
    {
        const double objectSpeed = gx_physics::norm(nativeVelocity),
                     handSpeed = gx_physics::norm(roomVelocity);
        if (objectSpeed >= .8 && objectSpeed <= gx_motion::GLITCH && handSpeed >= .8 &&
            handSpeed <= gx_motion::GLITCH &&
            gx_physics::dot(nativeVelocity, roomVelocity) > .55 * objectSpeed * handSpeed)
            return NATIVE_THROW;
    }
    return NATIVE_RELEASE;
}
inline void finalizeRelease(ReleaseReceipt& r, const Grasp& other)
{
    if (r.kind != HAND_TRANSFER)
        return;
    if (!other.held || other.epoch != r.epoch || !r.target.sameBody(other.token))
        r.kind = releaseKind(true, false, false, r.measured, r.nativeVelocity, r.roomVelocity);
}

// This optional velocity reference is a bounded force term, NEVER SetVelocity or
// a release impulse. Gain mass is explicitly limited, native mass stays real.
inline gx_physics::ForcePacket drive(const Grasp& grasp, const Frame& object, const Frame& hand,
                                     V nativeVelocity, double nativeMass, double simDt, bool door,
                                     bool natural, V roomVelocity, unsigned handCount, const V& center,
                                     const V* pointVelocity)
{
    if (!gx_physics::finite(nativeMass) || nativeMass <= 0 || nativeMass > (door ? 200 : 20) ||
        handCount < 1 || handCount > 2 || !pointVelocity)
        return gx_physics::ForcePacket();
    if (natural && (!gx_physics::finite(roomVelocity) || gx_physics::norm(roomVelocity) > gx_motion::GLITCH))
        return gx_physics::ForcePacket();
    const V reference = natural ? roomVelocity : V();
    V relative[3];
    for (unsigned i = 0; i < 3; ++i)
    {
        if (!gx_physics::finite(pointVelocity[i]))
            return gx_physics::ForcePacket();
        relative[i] = gx_physics::sub(pointVelocity[i], reference);
    }
    const double gainMass = nativeMass < 20 ? nativeMass : 20;
    return gx_physics::drive(grasp, object, hand, gx_physics::sub(nativeVelocity, reference), V(), gainMass,
                             simDt, 120 / handCount, 20 / handCount, 8 / handCount, &center, relative);
}
inline bool forcePoint(V point, V entityOrigin, V assertionOrigin, V worldCentroid, double radius, bool door)
{
    if (!door)
        return gx_physics::nativeForcePoint(point, entityOrigin, assertionOrigin, worldCentroid, radius);
    if (!gx_physics::finite(point) || !gx_physics::finite(entityOrigin) ||
        !gx_physics::finite(assertionOrigin) || !gx_physics::finite(worldCentroid) ||
        !gx_physics::finite(radius) || radius < .005 || radius > 20)
        return false;
    const V offset = gx_physics::sub(point, entityOrigin);
    return gx_physics::norm(offset) <= 2 &&
           gx_physics::norm(gx_physics::sub(gx_physics::add(assertionOrigin, offset), worldCentroid)) <=
               radius * 1.15;
}

class ContactEpisode
{
    Token target_;
    unsigned epoch_;
    double last_;
    bool touching_;
    gx_physics::ContactKind previousKind_;

  public:
    ContactEpisode() : epoch_(0), last_(-1), touching_(false), previousKind_(gx_physics::ACCIDENTAL) {}
    void separate(unsigned epoch)
    {
        if (epoch_ && epoch_ != epoch)
        {
            target_ = Token();
            last_ = -1;
        }
        epoch_ = epoch;
        touching_ = false;
        previousKind_ = gx_physics::ACCIDENTAL;
    }
    bool update(Token target, unsigned epoch, double time, gx_physics::ContactKind kind)
    {
        if (!target.valid() || !epoch || !gx_physics::finite(time))
        {
            *this = ContactEpisode();
            return false;
        }
        if (epoch_ && epoch_ != epoch)
        {
            *this = ContactEpisode();
            epoch_ = epoch;
            target_ = target;
            touching_ = true;
            previousKind_ = kind;
            return false;
        }
        const bool onset = !touching_ || !target_.sameBody(target) ||
                           (kind == gx_physics::PUSH && previousKind_ != gx_physics::PUSH);
        const bool issue = onset && kind == gx_physics::PUSH && (last_ < 0 || time - last_ >= 3);
        epoch_ = epoch;
        target_ = target;
        touching_ = true;
        previousKind_ = kind;
        if (issue)
            last_ = time;
        return issue;
    }
};

// Geometry coherence is not proof that these parts are dynamic solver bodies.
enum BodyPart
{
    PELVIS = 1u << 0,
    TORSO = 1u << 1,
    LEFT_ARM = 1u << 2,
    RIGHT_ARM = 1u << 3,
    LEFT_FOREARM = 1u << 4,
    RIGHT_FOREARM = 1u << 5,
    LEFT_HAND = 1u << 6,
    RIGHT_HAND = 1u << 7,
    LEFT_LEG = 1u << 8,
    RIGHT_LEG = 1u << 9,
    LEFT_FOOT = 1u << 10,
    RIGHT_FOOT = 1u << 11
};
struct BoundsGate
{
    bool enabled, local, human, onFoot, published, animated, alive, inLevel, inactive, updatesAnims;
    uint64_t poseEpoch, boundsEpoch;
    BoundsGate()
        : enabled(false), local(false), human(false), onFoot(false), published(false), animated(false),
          alive(false), inLevel(false), inactive(false), updatesAnims(false), poseEpoch(0), boundsEpoch(0)
    {
    }
};
inline bool updateBodyBounds(const BoundsGate& g)
{
    return g.enabled && g.local && g.human && g.onFoot && g.published && g.animated && g.alive && g.inLevel &&
           g.inactive && g.updatesAnims && g.poseEpoch && g.poseEpoch == g.boundsEpoch;
}
struct BoundsAudit
{
    unsigned components, mapped, invalid, covered, matching;
    double positionError, basisError;
    BoundsAudit()
        : components(0), mapped(0), invalid(0), covered(0), matching(0), positionError(0), basisError(0)
    {
    }
    void add(bool validMap, unsigned part, const Frame& expected, const Frame& actual)
    {
        ++components;
        if (!validMap || !expected.rigid() || !actual.rigid())
        {
            ++invalid;
            return;
        }
        ++mapped;
        covered |= part;
        const double p = gx_physics::norm(gx_physics::sub(expected.p, actual.p));
        double b = 0;
        const V e[3] = {expected.a, expected.b, expected.c}, a[3] = {actual.a, actual.b, actual.c};
        for (unsigned i = 0; i < 3; ++i)
        {
            const double err = gx_physics::norm(gx_physics::sub(e[i], a[i]));
            if (err > b)
                b = err;
        }
        if (p > positionError)
            positionError = p;
        if (b > basisError)
            basisError = b;
        if (p <= .001 && b <= .001)
            matching |= part;
    }
    bool coherent() const
    {
        return components && components == mapped && !invalid && positionError <= .001 && basisError <= .001;
    }
};
}
