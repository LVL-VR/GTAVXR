#pragma once
// Diagnostic value receipts only. No simulation, damage, task, input or haptic API.
#include "../physics/physics_contact.h"
namespace gx_armed_witness
{
enum
{
    MAX_LEAVES = 16,
    MAX_HITS = 16,
    MAX_NPCS = 8
};
// This is a query candidate budget, NOT a collision shape or hit criterion.
static const double TARGET_RADIUS = 3.0;
enum Status
{
    DISABLED,
    PROVIDER_MISSING,
    NOT_CURRENT,
    UNSUPPORTED,
    QUERY_FAILED,
    CAPACITY,
    CLEAR,
    CONTACT
};
struct Key
{
    uintptr_t actor, weapon, object, skeleton, layout;
    unsigned actorEpoch, weaponHandle, objectHandle, hand, attachBone, nativeFrame;
    uint64_t frame, generation;
    int64_t displayTime;
    Key()
        : actor(0), weapon(0), object(0), skeleton(0), layout(0), actorEpoch(0), weaponHandle(0),
          objectHandle(0), hand(2), attachBone(0), nativeFrame(0), frame(0), generation(0), displayTime(0)
    {
    }
    bool valid() const
    {
        return actor && weapon && object && skeleton && layout && actorEpoch && hand < 2 && nativeFrame &&
               frame && generation && displayTime > 0;
    }
    bool same(const Key& k) const
    {
        return actor == k.actor && weapon == k.weapon && object == k.object && skeleton == k.skeleton &&
               layout == k.layout && actorEpoch == k.actorEpoch && weaponHandle == k.weaponHandle &&
               objectHandle == k.objectHandle && hand == k.hand && attachBone == k.attachBone &&
               nativeFrame == k.nativeFrame && frame == k.frame && generation == k.generation &&
               displayTime == k.displayTime;
    }
};
inline bool rigid(const float* m)
{
    if (!m)
        return false;
    for (unsigned i = 0; i < 12; ++i)
        if (!gx_physics::finite(m[i]))
            return false;
    const gx_physics::V a(m[0], m[1], m[2]), b(m[3], m[4], m[5]), c(m[6], m[7], m[8]);
    return fabs(gx_physics::dot(a, a) - 1) < .01 && fabs(gx_physics::dot(b, b) - 1) < .01 &&
           fabs(gx_physics::dot(c, c) - 1) < .01 && fabs(gx_physics::dot(a, b)) < .01 &&
           fabs(gx_physics::dot(a, c)) < .01 && fabs(gx_physics::dot(b, c)) < .01 &&
           gx_physics::dot(gx_hands::cross(a, b), c) > .98;
}
inline bool sameMatrix(const float* a, const float* b, double tolerance = .002)
{
    if (!rigid(a) || !rigid(b) || !gx_physics::finite(tolerance) || tolerance < 0 || tolerance > .002)
        return false;
    for (unsigned i = 0; i < 12; ++i)
        if (fabs(double(a[i]) - b[i]) > tolerance)
            return false;
    return true;
}
inline bool compose(const float* local, const float* root, float* out)
{
    if (!out || !rigid(local) || !rigid(root))
        return false;
    for (unsigned c = 0; c < 4; ++c)
        for (unsigned r = 0; r < 3; ++r)
            out[c * 3 + r] = local[c * 3] * root[r] + local[c * 3 + 1] * root[3 + r] +
                             local[c * 3 + 2] * root[6 + r] + (c == 3 ? root[9 + r] : 0.f);
    return rigid(out);
}
struct Applied
{
    Key key;
    uint64_t serial, tick;
    float handWorld[12], attach[12], expectedRoot[12];
    bool written;
    Applied() : serial(0), tick(0), written(false)
    {
        memset(handWorld, 0, sizeof(handWorld));
        memset(attach, 0, sizeof(attach));
        memset(expectedRoot, 0, sizeof(expectedRoot));
    }
    bool capture(const Key& k, uint64_t s, uint64_t now, const float* actualWrittenHand,
                 const float* nativeAttach)
    {
        *this = Applied();
        if (!k.valid() || !s || !now || !compose(nativeAttach, actualWrittenHand, expectedRoot))
            return false;
        key = k;
        serial = s;
        tick = now;
        memcpy(handWorld, actualWrittenHand, sizeof(handWorld));
        memcpy(attach, nativeAttach, sizeof(attach));
        written = true;
        return true;
    }
    bool current(const Key& k, uint64_t now, const float* finalHand, const float* finalRoot,
                 const float* currentAttach, bool bodyPublished) const
    {
        return written && bodyPublished && key.same(k) && now >= tick && now - tick <= 100 &&
               sameMatrix(handWorld, finalHand) && sameMatrix(expectedRoot, finalRoot) &&
               sameMatrix(attach, currentAttach, 1e-4);
    }
};
struct Leaf
{
    uintptr_t instance, whole, bound;
    unsigned objectHandle, component, lod, level, generation, classType;
    float world[12], relative[12];
    Leaf()
        : instance(0), whole(0), bound(0), objectHandle(0), component(0), lod(0), level(65535), generation(0),
          classType(0)
    {
        memset(world, 0, sizeof(world));
        memset(relative, 0, sizeof(relative));
    }
    bool valid() const
    {
        return instance && whole && bound && component < MAX_LEAVES && generation <= 65535 &&
               level <= 65535 && rigid(world) && rigid(relative);
    }
    bool same(const Leaf& l) const
    {
        return valid() && l.valid() && instance == l.instance && whole == l.whole && bound == l.bound &&
               objectHandle == l.objectHandle && component == l.component && lod == l.lod &&
               level == l.level && generation == l.generation && classType == l.classType &&
               sameMatrix(world, l.world, 1e-4) && sameMatrix(relative, l.relative, 1e-4);
    }
};
struct Target
{
    uintptr_t actor, instance, whole;
    unsigned actorHandle, lod, children;
    uint64_t poseFingerprint;
    gx_physics::Token token;
    Target() : actor(0), instance(0), whole(0), actorHandle(0), lod(0), children(0), poseFingerprint(0) {}
    bool valid() const
    {
        return actor && instance && whole && children && children <= 64 && token.valid();
    }
    bool same(const Target& t) const
    {
        return valid() && t.valid() && actor == t.actor && actorHandle == t.actorHandle &&
               instance == t.instance && whole == t.whole && lod == t.lod && children == t.children &&
               poseFingerprint == t.poseFingerprint && token.sameBody(t.token);
    }
};
struct Hit
{
    Target target;
    uintptr_t targetLeaf;
    unsigned targetComponent, part;
    uint64_t material;
    gx_physics::V point, normal;
    double depth;
    float targetWorld[12];
    Hit() : targetLeaf(0), targetComponent(0), part(0), material(0), depth(0)
    {
        memset(targetWorld, 0, sizeof(targetWorld));
    }
    bool valid() const
    {
        return target.valid() && targetLeaf && targetComponent <= 65535 && part <= 65535 &&
               gx_physics::finite(point) && gx_physics::finite(normal) && gx_physics::finite(depth) &&
               depth > 0 && fabs(gx_physics::dot(normal, normal) - 1) <= .02 && rigid(targetWorld);
    }
};
struct Query
{
    Status status;
    unsigned count;
    Hit hits[MAX_HITS];
    Query() : status(QUERY_FAILED), count(0) {}
};
struct Receipt
{
    Status status;
    Key key;
    uint64_t appliedSerial;
    Leaf attacker;
    Hit hit;
    unsigned queries, targets;
    bool targetLimitReached;
    Receipt() : status(DISABLED), appliedSerial(0), queries(0), targets(0), targetLimitReached(false) {}
};
// Atomic publication gate. Clear stale/contact state before every attempt.
// Every hit is a current overlap, never a TOI/strike/solver impulse.
inline bool publish(const Applied& applied, const Key& live, uint64_t now, const float* hand,
                    const float* root, const float* attach, bool bodyPublished, const Leaf& before,
                    const Leaf& after, const Query& query, unsigned hitIndex, Receipt& out)
{
    out = Receipt();
    out.status = NOT_CURRENT;
    if (!applied.current(live, now, hand, root, attach, bodyPublished) || !before.same(after))
        return false;
    out.key = live;
    out.appliedSerial = applied.serial;
    out.attacker = before;
    out.queries = 1;
    if (query.status == CLEAR && query.count == 0)
    {
        out.status = CLEAR;
        return true;
    }
    if (query.status != CONTACT || !query.count || query.count >= MAX_HITS || hitIndex >= query.count)
    {
        out.status = query.status == CAPACITY ? CAPACITY : QUERY_FAILED;
        return false;
    }
    for (unsigned i = 0; i < query.count; ++i)
        if (!query.hits[i].valid() || query.hits[i].target.actor == live.actor)
        {
            out = Receipt();
            out.status = QUERY_FAILED;
            return false;
        }
    out.status = CONTACT;
    out.hit = query.hits[hitIndex];
    return true;
}
}
