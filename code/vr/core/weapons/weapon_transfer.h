#pragma once
// Original GTAVXR policy. POD in/out, no game, OS, allocation or input writes.
#include <stdint.h>
#include <float.h>
#include <math.h>
namespace gx_transfer
{
enum Owner
{
    FREE,
    LOST,
    MENU,
    PHONE,
    HOLSTER,
    PHYSICS,
    PIN,
    RELOAD,
    WEAPON,
    SUPPORT,
    TRANSFER
};
struct Claims
{
    bool live, menu, wheel;
    unsigned tracked, phone, shoulder, physics, pin, reload, weapon, support, transfer;
    Claims()
        : live(false), menu(false), wheel(false), tracked(0), phone(0), shoulder(0), physics(0), pin(0),
          reload(0), weapon(0), support(0), transfer(0)
    {
    }
};
struct Arbitration
{
    Owner hand[2];
    unsigned transferMask;
    Arbitration() : transferMask(0)
    {
        hand[0] = hand[1] = FREE;
    }
};
// One owner per hand. Existing actual holders beat new acquisitions. Transfer
// can never acquire an occupied receiver. Trigger support beats transfer Grip.
inline Arbitration arbitrate(const Claims& i)
{
    Arbitration o;
    for (unsigned h = 0; h < 2; ++h)
    {
        const unsigned b = 1u << h;
        Owner v = FREE;
        if (!i.live || !(i.tracked & b))
            v = LOST;
        else if (i.menu)
            v = MENU;
        else if (i.phone & b)
            v = PHONE;
        else if (i.wheel)
            v = HOLSTER;
        else if (i.physics & b)
            v = PHYSICS;
        else if (i.shoulder & b)
            v = HOLSTER;
        else if (i.pin & b)
            v = PIN;
        else if (i.reload & b)
            v = RELOAD;
        else if (i.weapon & b)
            v = WEAPON;
        else if (i.support & b)
            v = SUPPORT;
        else if (i.transfer & b)
            v = TRANSFER;
        o.hand[h] = v;
        if (v == TRANSFER)
            o.transferMask |= b;
    }
    return o;
}
enum Phase
{
    DRAIN,
    READY,
    OVERLAP
};
struct State
{
    Phase phase;
    uint64_t actor, object, weapon, sequence, pose, now, started;
    unsigned epoch, donor;
    State()
        : phase(DRAIN), actor(0), object(0), weapon(0), sequence(0), pose(0), now(0), started(0), epoch(0),
          donor(2)
    {
    }
};
struct Sample
{
    Claims claims;
    uint64_t actor, object, weapon, sequence, pose, now;
    unsigned epoch, donor;
    bool transferable, receiverEmpty, geometry, legacyReady;
    double distance;
    float grip[2], trigger[2];
    Sample()
        : actor(0), object(0), weapon(0), sequence(0), pose(0), now(0), epoch(0), donor(2),
          transferable(false), receiverEmpty(false), geometry(false), legacyReady(false), distance(0)
    {
        grip[0] = grip[1] = trigger[0] = trigger[1] = 0;
    }
};
struct Step
{
    State state;
    Arbitration owners;
    bool commit, cancelled;
    unsigned reserve;
    Step() : commit(false), cancelled(false), reserve(0) {}
};
inline bool finiteSample(const Sample& i)
{
    return _finite(i.distance) && i.distance >= 0 && _finite(i.grip[0]) && _finite(i.grip[1]) &&
           _finite(i.trigger[0]) && _finite(i.trigger[1]);
}
// Geometry: receiver OpenXR grip center to WEAPON_GRIP_R on the actual posed
// donor object, meters. Enter <=110 mm, remain <=160 mm. No root fallback.
// Receiver fresh Grip >=.70, donor initially >=.70; receiver hold >=.45.
// Donor release <.25 after 120..1200 ms. Both Triggers must remain <.20.
inline Step advance(const State& before, const Sample& i)
{
    Step o;
    o.state = before;
    Claims c = i.claims;
    const bool identified =
        i.actor && i.epoch && i.weapon && i.object && i.sequence && i.pose && i.now && i.donor < 2;
    const bool same = identified && before.actor == i.actor && before.epoch == i.epoch &&
                      before.weapon == i.weapon && before.object == i.object && before.donor == i.donor;
    const bool ordered =
        same && i.sequence >= before.sequence && i.pose >= before.pose && i.now >= before.now;
    if (!ordered)
        o.state = State();
    o.state.actor = i.actor;
    o.state.epoch = i.epoch;
    o.state.weapon = i.weapon;
    o.state.object = i.object;
    o.state.donor = i.donor;
    o.state.sequence = i.sequence;
    o.state.pose = i.pose;
    o.state.now = i.now;
    const unsigned receiver = i.donor < 2 ? 1 - i.donor : 0, rb = 1u << receiver;
    c.transfer |= rb;
    o.owners = arbitrate(c);
    const bool available = identified && finiteSample(i) && i.claims.tracked == 3 && i.claims.live &&
                           i.transferable && i.receiverEmpty && i.geometry && i.legacyReady &&
                           o.owners.hand[receiver] == TRANSFER && o.owners.hand[i.donor] == WEAPON &&
                           i.trigger[0] < .20f && i.trigger[1] < .20f;
    if (!available)
    {
        o.cancelled = before.phase == OVERLAP;
        o.state.phase = DRAIN;
        o.state.started = 0;
        return o;
    }
    if (o.state.phase == DRAIN)
    {
        if (i.grip[receiver] < .25f && i.grip[i.donor] >= .70f)
            o.state.phase = READY;
        return o;
    }
    // Identical input may have a newer pose; it cannot create another edge or
    // complete a transfer. Invalidity above still cancels it immediately.
    if (ordered && i.sequence == before.sequence)
    {
        if (o.state.phase == OVERLAP)
        {
            if (i.distance > .160 || i.now - o.state.started > 1200)
            {
                o.cancelled = true;
                o.state.phase = DRAIN;
            }
            else
                o.reserve = rb;
        }
        return o;
    }
    if (o.state.phase == READY)
    {
        if (i.grip[i.donor] < .70f)
        {
            o.state.phase = DRAIN;
            return o;
        }
        if (i.grip[receiver] >= .70f)
        {
            if (i.distance <= .110)
            {
                o.state.phase = OVERLAP;
                o.state.started = i.now;
                o.reserve = rb;
            }
            else
                o.state.phase = DRAIN; // held Grip cannot slide into activation
        }
        return o;
    }
    const uint64_t age = i.now - o.state.started;
    if (i.distance > .160 || i.grip[receiver] < .45f || age > 1200)
    {
        o.cancelled = true;
        o.state.phase = DRAIN;
        return o;
    }
    o.reserve = rb;
    if (i.grip[i.donor] < .25f)
    {
        o.commit = age >= 120;
        o.cancelled = !o.commit;
        o.state.phase = DRAIN;
        o.reserve = 0;
        o.state.started = 0;
    }
    return o;
}
// Bounded receipt compares values immediately around adoption, in one UPDATE.
struct Snapshot
{
    uint64_t object, weapon, components;
    int clip, total, pool, state, reloadMs;
    unsigned count, tint, timer, clock, nextAllowed, cook;
    bool cooking;
    Snapshot()
        : object(0), weapon(0), components(0), clip(0), total(0), pool(0), state(0), reloadMs(0), count(0),
          tint(0), timer(0), clock(0), nextAllowed(0), cook(0), cooking(false)
    {
    }
};
inline bool conserved(const Snapshot& a, const Snapshot& b)
{
    return a.object == b.object && a.weapon == b.weapon && a.components == b.components && a.clip == b.clip &&
           a.total == b.total && a.pool == b.pool && a.state == b.state && a.reloadMs == b.reloadMs &&
           a.count == b.count && a.tint == b.tint && a.timer == b.timer && a.clock == b.clock &&
           a.nextAllowed == b.nextAllowed && a.cook == b.cook && a.cooking == b.cooking;
}
}
