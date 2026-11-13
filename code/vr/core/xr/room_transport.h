#pragma once
// GTAVXR world-origin ownership. No guessed "accepted room" decomposition.
// Native task transport and room-follow transport are mutually exclusive requests.
// Tracking is never consumed by a request or by failure of the native collider.
#include "locomotion.h"
namespace gx_room
{
using gx_loco::Vec2;
inline Vec2 add(Vec2 a, Vec2 b)
{
    return Vec2(a.x + b.x, a.y + b.y);
}
inline Vec2 sub(Vec2 a, Vec2 b)
{
    return Vec2(a.x - b.x, a.y - b.y);
}
inline Vec2 scale(Vec2 a, double b)
{
    return Vec2(a.x * b, a.y * b);
}
inline bool finite(Vec2 a)
{
    return gx_loco::finite(a.x) && gx_loco::finite(a.y);
}
inline double norm(Vec2 a)
{
    return sqrt(a.x * a.x + a.y * a.y);
}
// XR x/z -> native horizontal x/y. This orthogonal/reflected basis is its inverse.
inline Vec2 map(Vec2 a, double yaw)
{
    const double c = cos(yaw), s = sin(yaw);
    return Vec2(c * a.x + s * a.y, s * a.x - c * a.y);
}
enum Owner
{
    NO_OWNER,
    NATIVE_TRANSPORT,
    ROOM_FOLLOW,
    MIXED_UNQUALIFIED
};
struct Request
{
    Vec2 additive;
    Owner owner;
    bool discontinuity;
    Request() : owner(NO_OWNER), discontinuity(false) {}
};
struct Receipt
{
    Vec2 bodyDelta, originDelta;
    Owner owner;
    bool valid, duplicate;
    Receipt() : owner(NO_OWNER), valid(false), duplicate(false) {}
};
class Transport
{
    Vec2 trackingOrigin_, worldOrigin_, lastBody_, preBody_, lastPhysical_;
    bool anchored_, worldReady_, pending_, sampleReady_, posted_;
    unsigned pendingFrame_, postFrame_, epoch_;
    uint64_t sampleSequence_;
    Owner pendingOwner_;
    unsigned generation_; // build168 item 6: counts origin re-seeds (reset / anchor / respawn)
  public:
    Transport() : generation_(0)
    {
        reset();
    }
    void reset()
    {
        ++generation_;
        trackingOrigin_ = worldOrigin_ = lastBody_ = preBody_ = lastPhysical_ = Vec2();
        anchored_ = worldReady_ = pending_ = sampleReady_ = posted_ = false;
        pendingFrame_ = postFrame_ = epoch_ = 0;
        sampleSequence_ = 0;
        pendingOwner_ = NO_OWNER;
    }
    void anchor(Vec2 physical)
    {
        reset();
        if (finite(physical))
        {
            trackingOrigin_ = lastPhysical_ = physical;
            anchored_ = true;
        }
    }
    bool ready() const
    {
        return anchored_ && worldReady_;
    }
    void suspend()
    {
        pending_ = false;
        pendingOwner_ = NO_OWNER;
    } // no re-anchor, tracking remains free.
    bool seed(Vec2 body, unsigned epoch)
    {
        if (!anchored_ || !finite(body) || !epoch)
            return false;
        if (worldReady_ && epoch_ != epoch)
            return false; // identity owner must explicitly reset/re-anchor.
        if (!worldReady_)
        {
            worldOrigin_ = lastBody_ = body;
            epoch_ = epoch;
            worldReady_ = true;
        }
        return true;
    }
    Vec2 worldTarget(Vec2 physical, double yaw) const
    {
        return add(worldOrigin_, map(sub(physical, trackingOrigin_), yaw));
    }
    Vec2 reference(Vec2 body, double yaw) const
    {
        return add(trackingOrigin_, map(sub(body, worldOrigin_), yaw));
    }
    Vec2 worldOrigin() const
    {
        return worldOrigin_;
    }
     
    // follows by exactly that movement (position only; tracking origin, yaw and physical lean unchanged).
    bool carry(Vec2 delta, unsigned epoch)
    {
        if (!ready() || !finite(delta) || epoch != epoch_)
            return false;
        worldOrigin_ = add(worldOrigin_, delta);
        return true;
    }
    bool lastBody(Vec2& out) const
    {
        if (!ready() || !posted_)
            return false;
        out = lastBody_;
        return true;
    }
    // build169 item 1: the last posted body only when it was posted this frame or the previous one (else false)
    bool freshLastBody(Vec2& out, unsigned frame) const
    {
        if (!ready() || !posted_ || (unsigned)(frame - postFrame_) > 1u)
            return false;
        out = lastBody_;
        return true;
    }
    unsigned generation() const
    {
        return generation_;
    }
    // Completed native task/attachment travel only. A caller with explicit
    // native ownership may reconcile a final transform even if the mover
    // receipt was skipped. Never use this as a generic body-delta fallback.
    Receipt settleNative(Vec2 body, unsigned frame, unsigned epoch)
    {
        Receipt out;
        if (!ready() || !finite(body) || epoch != epoch_)
            return out;
        if (posted_ && (unsigned)(frame - postFrame_) > 0x7fffffffu)
            return out;
        // A live room-follow request cannot be re-labelled as native travel:
        // that would consume physical tracking against a wall a second time.
        if (pending_ && pendingFrame_ == frame && pendingOwner_ == ROOM_FOLLOW)
            return out;
        out.owner = NATIVE_TRANSPORT;
        out.bodyDelta = sub(body, lastBody_);
        out.originDelta = out.bodyDelta;
        out.duplicate = posted_ && frame == postFrame_ && norm(out.bodyDelta) == 0;
        worldOrigin_ = add(worldOrigin_, out.originDelta);
        lastBody_ = body;
        postFrame_ = frame;
        posted_ = true;
        suspend();
        out.valid = true;
        return out;
    }

    bool nativeAction(unsigned frame, unsigned epoch)
    {
        if (!ready() || epoch != epoch_ || (posted_ && frame == postFrame_))
            return false;
        // Explicit native root-motion task, NOT a guessed large body delta.
        preBody_ = lastBody_;
        pendingFrame_ = frame;
        pendingOwner_ = NATIVE_TRANSPORT;
        pending_ = true;
        return true;
    }
    bool respawn(Vec2 body, Vec2 physical, unsigned epoch)
    {
        if (!finite(body) || !finite(physical) || !epoch)
            return false;
        anchor(physical);
        return seed(body, epoch);
    }
    void turn(double oldYaw, double newYaw, Vec2 physical)
    {
        if (!ready() || !finite(physical) || !gx_loco::finite(oldYaw) || !gx_loco::finite(newYaw))
            return;
        // Turn about the CURRENT HMD, including when the collider is left behind.
        worldOrigin_ = sub(worldTarget(physical, oldYaw), map(sub(physical, trackingOrigin_), newYaw));
    }
    Request request(Vec2 body, Vec2 physical, Vec2 nativeVelocity, double yaw, uint64_t sequence,
                    unsigned frame, unsigned epoch, double simulationDt, double physicalDt, bool roomEligible,
                    bool nativeCarrier)
    {
        Request out;
        if (!finite(body) || !finite(physical) || !finite(nativeVelocity) || !gx_loco::finite(yaw) ||
            !sequence || !seed(body, epoch))
        {
            suspend();
            return out;
        }
        if (posted_ && frame == postFrame_)
        {
            out.discontinuity = true;
            return out;
        }
        if (posted_ && (unsigned)(frame - postFrame_) > 0x7fffffffu)
        {
            out.discontinuity = true;
            return out;
        }
        out.owner = !roomEligible || nativeCarrier || nativeVelocity.x != 0 || nativeVelocity.y != 0
                        ? NATIVE_TRANSPORT
                        : ROOM_FOLLOW;
        if (pending_ && frame != pendingFrame_)
        {
            // Missing post-solver receipt is not accepted motion. Retire the old
            // request without changing the origin; diagnose at the native seam.
            pending_ = false;
            out.discontinuity = true;
        }
        if (!pending_)
        {
            preBody_ = body;
            pendingFrame_ = frame;
            pendingOwner_ = out.owner;
            pending_ = true;
        }
        else if (pendingOwner_ != out.owner)
        {
            pendingOwner_ = MIXED_UNQUALIFIED;
            out.discontinuity = true;
        }
        if (pendingOwner_ == MIXED_UNQUALIFIED)
        {
            out.owner = pendingOwner_;
            return out;
        }
        if (!gx_loco::finite(simulationDt) || simulationDt < .0001 || simulationDt > .2 ||
            !gx_loco::finite(physicalDt) || physicalDt <= 0 || physicalDt > .2)
        {
            out.discontinuity = true;
            return out;
        }
        if (sampleReady_ && sequence < sampleSequence_)
        {
            out.discontinuity = true;
            return out;
        }
        if (!sampleReady_ || sequence > sampleSequence_)
        {
            const Vec2 sampleDelta = sub(physical, lastPhysical_);
            out.discontinuity =
                out.discontinuity ||
                (sampleReady_ && (norm(sampleDelta) > .25 || norm(sampleDelta) > 5 * physicalDt));
            lastPhysical_ = physical;
            sampleSequence_ = sequence;
            sampleReady_ = true;
        }
        if (out.owner != ROOM_FOLLOW || out.discontinuity)
            return out;
        const Vec2 separation = sub(worldTarget(physical, yaw), body);
        const double distance = norm(separation);
        if (!gx_loco::finite(distance))
        {
            out.discontinuity = true;
            return out;
        }
        if (distance <= .001)
            return out;
        // Bounded native following; not a teleport/catch-up impulse. Simulation
        // rate only changes this request, never the rendered tracked pose.
        double speed = distance / simulationDt;
        const double cap = 3 * physicalDt / simulationDt < 20 ? 3 * physicalDt / simulationDt : 20;
        if (speed > cap)
            speed = cap;
        out.additive = scale(separation, speed / distance);
        return out;
    }
    Receipt post(Vec2 body, unsigned frame, unsigned epoch)
    {
        Receipt out;
        if (!ready() || !finite(body) || epoch != epoch_)
            return out;
        if (posted_ && frame == postFrame_)
        {
            out.duplicate = true;
            return out;
        }
        if (posted_ && (unsigned)(frame - postFrame_) > 0x7fffffffu)
            return out;
        if (pending_ && pendingFrame_ == frame)
        {
            out.owner = pendingOwner_;
            out.bodyDelta = sub(body, preBody_);
        }
        else
        {
            // No matching producer receipt is NOT proof of ordinary native
            // transport: an old room velocity can persist through a skipped
            // mover update. Reject attribution instead of moving the eye twice.
            out.owner = MIXED_UNQUALIFIED;
            out.bodyDelta = sub(body, lastBody_);
        }
        if (out.owner == NATIVE_TRANSPORT)
        {
            out.originDelta = out.bodyDelta;
            worldOrigin_ = add(worldOrigin_, out.originDelta);
        }
        // ROOM_FOLLOW moves the collider toward the tracked target; it must NOT
        // move the target/world origin a second time. Mixed ownership is rejected.
        lastBody_ = body;
        postFrame_ = frame;
        posted_ = true;
        suspend();
        out.valid = true;
        return out;
    }
};
}
