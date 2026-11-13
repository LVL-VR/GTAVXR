#pragma once
// On-foot native weapon ownership and trigger intent. No combat, ammo mutation,
 
#include <stdint.h>
#include <float.h>
namespace gx_weapon_hands
{
enum Hand
{
    LEFT = 0,
    RIGHT = 1
};
enum Owner
{
    FREE,
    WEAPON,
    SUPPORT,
    PHONE,
    VEHICLE,
    PHYSICS,
    MISSION,
    MENU
};
enum Action
{
    NONE,
    FIRE,
    REQUEST_SUPPORT,
    RELEASE_SUPPORT
};
enum Family
{
    UNARMED,
    HITSCAN,
    PROJECTILE,
    THROWN,
    VOLUMETRIC,
    MELEE,
    UNKNOWN_FAMILY
};
struct Item
{
    uint64_t instance;
    unsigned hash, ammoHash;
    Family family;
    bool twoHanded, oneShotPerPress, streamed, nativeReady;
    Item()
        : instance(0), hash(0), ammoHash(0), family(UNARMED), twoHanded(false), oneShotPerPress(false),
          streamed(false), nativeReady(false)
    {
    }
    bool armed() const
    {
        return instance && hash && streamed && family != UNARMED;
    }
};
struct Input
{
    uint64_t actor, sequence, generation, poseFrame;
    unsigned epoch, hands, blockedHands;
    bool live, onFoot, paused, dead, ragdoll, online, missionBlocked, menu, wheel;
    float trigger[2];
    Owner owner[2];
    Item item[2];
    Input()
        : actor(0), sequence(0), generation(0), poseFrame(0), epoch(0), hands(0), blockedHands(0),
          live(false), onFoot(false), paused(false), dead(false), ragdoll(false), online(false),
          missionBlocked(false), menu(false), wheel(false)
    {
        trigger[0] = trigger[1] = 0;
        owner[0] = owner[1] = FREE;
    }
};
struct Decision
{
    Owner owner[2];
    Action action[2];
    unsigned fireMask, supportMask;
    bool active;
    Decision() : fireMask(0), supportMask(0), active(false)
    {
        owner[0] = owner[1] = FREE;
        action[0] = action[1] = NONE;
    }
};
inline bool eligible(const Input& i)
{
    return i.live && i.onFoot && !i.paused && !i.dead && !i.ragdoll && !i.online && !i.missionBlocked &&
           !i.menu && !i.wheel && i.actor && i.epoch && i.sequence && i.generation && i.poseFrame &&
           !(i.hands & ~3u) && !(i.blockedHands & ~3u) && _finite(i.trigger[0]) && _finite(i.trigger[1]);
}
inline bool available(const Input& i, unsigned h)
{
    return h < 2 && (i.hands & (1u << h)) && !(i.blockedHands & (1u << h)) &&
           (i.owner[h] == FREE || i.owner[h] == WEAPON || i.owner[h] == SUPPORT);
}
 
// engine frame. One attempt per hand/real instance prevents duplicate callback
// transactions without altering native cooldown or advancing native state.
class AttemptLedger
{
    uint64_t actor_, instance_[2];
    unsigned epoch_, frame_[2];

  public:
    AttemptLedger()
    {
        reset();
    }
    void reset()
    {
        actor_ = 0;
        epoch_ = 0;
        for (unsigned h = 0; h < 2; ++h)
        {
            instance_[h] = 0;
            frame_[h] = 0;
        }
    }
    bool begin(uint64_t actor, unsigned epoch, unsigned hand, uint64_t instance, unsigned frame)
    {
        if (!actor || !epoch || hand > 1 || !instance || !frame)
            return false;
        if (actor_ != actor || epoch_ != epoch)
        {
            reset();
            actor_ = actor;
            epoch_ = epoch;
        }
        if (instance_[hand] == instance && frame_[hand] == frame)
            return false;
        instance_[hand] = instance;
        frame_[hand] = frame;
        return true;
    }
};
class Resolver
{
    uint64_t actor_, sequence_, instance_[2], releaseFrame_[2];
    unsigned epoch_;
    bool held_[2], fired_[2], drain_[2], blocked_[2];

  public:
    Resolver() : actor_(0), sequence_(0), epoch_(0)
    {
        reset();
    }
    void reset()
    {
        actor_ = sequence_ = epoch_ = 0;
        for (unsigned h = 0; h < 2; ++h)
        {
            releaseFrame_[h] = instance_[h] = 0;
            held_[h] = fired_[h] = blocked_[h] = false;
            drain_[h] = true;
        }
    }
    Decision update(const Input& i)
    {
        Decision out;
        out.owner[0] = i.owner[0];
        out.owner[1] = i.owner[1];
        if (actor_ != i.actor || epoch_ != i.epoch || sequence_ > i.sequence)
        {
            reset();
            actor_ = i.actor;
            epoch_ = i.epoch;
        }
        sequence_ = i.sequence;
        if (!eligible(i))
        {
            for (unsigned h = 0; h < 2; ++h)
            {
                releaseFrame_[h] = 0;
                held_[h] = fired_[h] = blocked_[h] = false;
                drain_[h] = true;
            }
            return out;
        }
        out.active = true;
        bool released[2] = {false, false};
        for (unsigned h = 0; h < 2; ++h)
        {
            if (instance_[h] != i.item[h].instance)
            {
                instance_[h] = i.item[h].instance;
                releaseFrame_[h] = 0;
                held_[h] = fired_[h] = blocked_[h] = false;
                drain_[h] = true;
            }
            if (!available(i, h))
            {
                releaseFrame_[h] = 0;
                held_[h] = fired_[h] = blocked_[h] = false;
                drain_[h] = true;
                continue;
            }
            if (i.trigger[h] < .2f)
            {
                released[h] = held_[h] && !drain_[h];
                held_[h] = false;
                drain_[h] = false;
                blocked_[h] = false;
                if (released[h])
                {
                    fired_[h] = false;
                    releaseFrame_[h] = i.poseFrame;
                }
                else if (i.item[h].family != THROWN)
                    fired_[h] = false;
            }
            else if (i.trigger[h] >= .55f)
            {
                held_[h] = true;
                if (i.item[h].family == THROWN)
                    releaseFrame_[h] = 0;
            }
            if (releaseFrame_[h] && (i.poseFrame < releaseFrame_[h] || i.poseFrame - releaseFrame_[h] > 60))
                releaseFrame_[h] = 0;
        }
        // Two physically equipped native weapons always take precedence over
        // support, even when one is reloading or has no ammunition.
        for (unsigned h = 0; h < 2; ++h)
            if (available(i, h) && i.item[h].armed())
            {
                out.owner[h] = WEAPON;
                if ((i.item[h].family == THROWN && releaseFrame_[h] && !fired_[h]) ||
                    (i.item[h].family != MELEE && i.item[h].family != THROWN && !drain_[h] && held_[h] &&
                     !blocked_[h] && (!i.item[h].oneShotPerPress || !fired_[h])))
                {
                    out.action[h] = FIRE;
                    out.fireMask |= 1u << h;
                }
            }
        for (unsigned h = 0; h < 2; ++h)
        {
            const unsigned gun = 1 - h;
            if (!available(i, h) || i.item[h].armed() || !available(i, gun) || !i.item[gun].armed() ||
                !i.item[gun].twoHanded)
                continue;
            if (!drain_[h] && i.trigger[h] >= .2f)
            {
                out.owner[h] = SUPPORT;
                out.action[h] = REQUEST_SUPPORT;
                out.supportMask |= 1u << h;
            }
            else if (i.owner[h] == SUPPORT)
            {
                out.owner[h] = FREE;
                out.action[h] = RELEASE_SUPPORT;
            }
        }
        return out;
    }
     
    // shot does not consume the semi-auto edge. Cooldown/spread remain native.
    void fired(unsigned hand, uint64_t instance, bool nativeSuccess)
    {
        if (hand < 2 && nativeSuccess && instance && instance == instance_[hand])
        {
            fired_[hand] = true;
            releaseFrame_[hand] = 0;
        }
    }
    // build158 C2: a press refused because the weapon is not ready by OUR reload machines is spent (dry pull).
    // Firing again needs a release and a new press, for every family; it never fires later on its own.
    void blocked(unsigned hand, uint64_t instance)
    {
        if (hand < 2 && instance && instance == instance_[hand] && held_[hand])
            blocked_[hand] = true;
    }
};
// Inventory total includes loaded rounds in BOTH magazines. Never load the
// opposite magazine's rounds or duplicate a finite native ammo pool.
inline bool reserveOppositeClip(bool sameAmmo, bool ownThrown, bool otherThrown)
{
    return sameAmmo && !ownThrown && !otherThrown;
}
inline int availableForReload(int total, int ownClip, int otherClip, bool sameAmmo)
{
    if (total == -1)
        return -1;
    if (total < 0 || ownClip < 0 || otherClip < 0)
        return 0;
    const int64_t reserved = (int64_t)ownClip + (sameAmmo ? (int64_t)otherClip : 0);
    return (int64_t)total > reserved ? (int)((int64_t)total - reserved) : 0;
}
inline int newMagazine(int nativeLoaded, int total, int otherClip, bool sameAmmo)
{
    if (nativeLoaded < 0 || otherClip < 0 || total < -1)
        return 0;
    if (total == -1)
        return nativeLoaded;
    const int limit = total > (sameAmmo ? otherClip : 0) ? total - (sameAmmo ? otherClip : 0) : 0;
    return nativeLoaded < limit ? nativeLoaded : limit;
}
// Native wheel returns are asynchronous. Consume once with the captured hand,
// actor/epoch and concrete return ID, rather than querying hand after close.
class WheelSelection
{
    uint64_t actor_;
    unsigned epoch_, hand_, returnId_, deadline_;
    bool pending_;

  public:
    WheelSelection() : actor_(0), epoch_(0), hand_(0), returnId_(0), deadline_(0), pending_(false) {}
    void reset()
    {
        actor_ = 0;
        epoch_ = hand_ = returnId_ = deadline_ = 0;
        pending_ = false;
    }
    bool arm(uint64_t actor, unsigned epoch, unsigned hand, unsigned id, unsigned frame)
    {
        if (!actor || !epoch || hand > 1 || !id || !frame)
        {
            reset();
            return false;
        }
        actor_ = actor;
        epoch_ = epoch;
        hand_ = hand;
        returnId_ = id;
        deadline_ = frame + 8;
        pending_ = true;
        return true;
    }
    bool take(uint64_t actor, unsigned epoch, unsigned id, unsigned frame, unsigned& hand)
    {
        if (!pending_)
            return false;
        if (actor != actor_ || epoch != epoch_ || (int32_t)(frame - deadline_) > 0 || !frame)
        {
            reset();
            return false;
        }
        if (id != returnId_)
            return false;
        hand = hand_;
        reset();
        return true;
    }
    bool pending() const
    {
        return pending_;
    }
};
// Native shot attribution, not an invented pad-motor-to-hand mapping.
struct ShotPulse
{
    uint64_t sequence;
    unsigned epoch, hand;
    float amplitude;
    unsigned durationMs;
    ShotPulse() : sequence(0), epoch(0), hand(0), amplitude(0), durationMs(0) {}
};
inline bool validPulse(const ShotPulse& p, unsigned epoch)
{
    return p.sequence && p.epoch && p.epoch == epoch && p.hand < 2 && _finite(p.amplitude) &&
           p.amplitude > 0 && p.amplitude <= 1 && p.durationMs && p.durationMs <= 10000;
}
}
