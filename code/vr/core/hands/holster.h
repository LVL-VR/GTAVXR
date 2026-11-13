#pragma once
 
#include "../core/input.h"
#include "../core/hands.h"
#include "../xr/locomotion.h"
namespace gx_holster
{
enum
{
    PageSize = 8
};
inline bool nearShoulder(const GxPose& head, const GxPose& hand, unsigned side)
{
    const double dx = hand.x - head.x, dy = hand.y - head.y, dz = hand.z - head.z;
    if (side > 1 || !gx_loco::finite(dx) || !gx_loco::finite(dy) || !gx_loco::finite(dz))
        return false;
    const double yaw = gx_loco::yaw(head), c = cos(yaw), s = sin(yaw);
    const double x = c * dx - s * dz, z = s * dx + c * dz, sideX = side ? x : -x;
    // Head/ear volume only. Below-head torso/back gestures are NOT a holster.
    return sideX > .055 && sideX < .36 && dy > -.17 && dy < .19 && z > -.035 && z < .30 &&
           dx * dx + dy * dy + dz * dz < .39 * .39;
}
 
// hacer el gesto con la mano vacia ... el ultimo arma que tuve en esa mano"): a grip pressed at the holster and released within
// TAP_S without a selection is a tap (tapped, tapHand).
 
// wheel opens AT the press again, as before 160. A release within TAP_S with no stick selection and no click closes it as a
// cancel (no weapon chosen, exactly what a quick release did before 160) and reports the tap.
inline double TAP_S()
{
    return .35;
}
struct Result
{
    bool consume, opened, cancelled, released, nextInGroup, tapped;
    int choice, tapHand;
    Result()
        : consume(false), opened(false), cancelled(false), released(false), nextInGroup(false), tapped(false),
          choice(-1), tapHand(-1)
    {
    }
};
class Wheel
{
    int hand_, choice_;
    unsigned count_, page_;
    bool barrier_, click_;
    // build161 B: when the open wheel can still become a tap (opened at a timed press, nothing selected yet), since when
    // per-hand grip level of the last update (a gesture starts only on a grip PRESS at the holster, Codex reviewAH160 R4)
    bool tapCandidate_;
    double openedAt_;
    bool gripHigh_[2];
    void open(int h, unsigned count, const GxInput& i)
    {
        hand_ = h;
        count_ = count;
        page_ = 0;
        choice_ = -1;
        click_ = (i.buttons & (h ? GX_RCLICK : GX_LCLICK)) != 0;
    }

  public:
    Wheel()
        : hand_(-1), choice_(-1), count_(0), page_(0), barrier_(true), click_(false), tapCandidate_(false),
          openedAt_(0)
    {
        gripHigh_[0] = gripHigh_[1] = true;
    }
    bool tapCandidate() const
    {
        return visible() && tapCandidate_;
    }
    bool visible() const
    {
        return hand_ >= 0;
    }
    int hand() const
    {
        return hand_;
    }
    unsigned page() const
    {
        return page_;
    }
    int choice() const
    {
        return choice_;
    }
    unsigned rows() const
    {
        const unsigned left = count_ > page_ * PageSize ? count_ - page_ * PageSize : 0;
        return left > PageSize ? PageSize : left;
    }
    void cancel()
    {
        hand_ = -1;
        choice_ = -1;
        barrier_ = true;
        click_ = false;
        tapCandidate_ = false;
    }
    // now < 0 (the pre-160 callers and controls): the wheel opens at the grip press, no tap
    Result update(const GxInput& i, const GxHandTargets& poses, bool fresh, bool eligible, unsigned count,
                  bool nativeGroups = false, double now = -1)
    {
        Result out;
        bool neutral = i.buttons == 0;
        // grip level BEFORE this update, per hand (a press is low -> >= .7; high stays high until released below .25)
        bool wasHigh[2];
        for (unsigned h = 0; h < 2; ++h)
        {
            wasHigh[h] = gripHigh_[h];
            if (gx_loco::finite(i.grip[h]))
                gripHigh_[h] = i.grip[h] >= .7 ? true : (i.grip[h] < .25 ? false : gripHigh_[h]);
        }
        for (unsigned h = 0; h < 2; ++h)
        {
            if (!gx_loco::finite(i.grip[h]) || !gx_loco::finite(i.trigger[h]) ||
                !gx_loco::finite(i.stick[h][0]) || !gx_loco::finite(i.stick[h][1]))
            {
                cancel();
                out.consume = true;
                out.cancelled = true;
                return out;
            }
            neutral = neutral && i.grip[h] < .2 && i.trigger[h] < .2 && fabs(i.stick[h][0]) < .25 &&
                      fabs(i.stick[h][1]) < .25;
        }
        const bool valid = i.size == sizeof(i) && i.version == 1 && i.head_valid && fresh &&
                           poses.size == sizeof(poses) && poses.version == 1 && count > 0;
        if (!valid || !eligible)
        {
            out.cancelled = visible();
            out.consume = visible() || (barrier_ && !neutral);
            if (visible())
                cancel();
            else if (neutral)
                barrier_ = false;
            return out;
        }
        if (barrier_)
        {
            out.consume = true;
            if (neutral)
                barrier_ = false;
            return out;
        }
        if (!visible())
        {
            for (int h = 1; h >= 0; --h)
                if ((i.active_hands & poses.valid_mask & (1u << h)) && i.grip[h] >= .7 && !wasHigh[h] &&
                    !(i.buttons & (GX_MENU | GX_B | GX_Y)) && nearShoulder(i.head, poses.local[h], h))
                {
                    const double x = i.stick[h][0], y = i.stick[h][1];
                    open(h, count, i);
                    out.opened = out.consume = true;
                    // a stick already deflected at the press is a selection, never a tap (reviewAH160 R7)
                    // build161 v2 (Codex reviewABC161): a click held at the press is a selection too
                    tapCandidate_ =
                        now >= 0 && x * x + y * y < .55 * .55 && !(i.buttons & (h ? GX_RCLICK : GX_LCLICK));
                    openedAt_ = now;
                    return out;
                }
            return out;
        }
        out.consume = true;
        if (!(i.active_hands & poses.valid_mask & (1u << hand_)) || count != count_ ||
            i.buttons & (GX_MENU | GX_B | GX_Y))
        {
            cancel();
            out.cancelled = true;
            return out;
        }
        // build161 B: a quick release with nothing selected is the tap (the wheel closes as a cancel); R7: a release seen only
        // after TAP_S (a frame stall) is an ordinary release
        if (tapCandidate_ && (!(now >= openedAt_) || now - openedAt_ >= TAP_S()))
            tapCandidate_ = false;
        if (i.grip[hand_] < .25)
        {
            // build161 v2 (Codex reviewABC161): a stick deflection or a click in the release frame itself is no tap
            const double rx = i.stick[hand_][0], ry = i.stick[hand_][1];
            const bool releaseInput =
                rx * rx + ry * ry >= .55 * .55 || (i.buttons & (hand_ ? GX_RCLICK : GX_LCLICK)) != 0;
            if (tapCandidate_ && choice_ < 0 && !releaseInput)
            {
                out.tapped = true;
                out.tapHand = hand_;
                out.cancelled = true;
                cancel();
                return out;
            }
            out.released = true;
            out.choice = choice_;
            cancel();
            return out;
        }
        const bool click = (i.buttons & (hand_ ? GX_RCLICK : GX_LCLICK)) != 0;
        if (click && !click_)
        {
            tapCandidate_ = false;
            if (nativeGroups)
                out.nextInGroup = true;
            else
            {
                page_ = (page_ + 1) % ((count_ + PageSize - 1) / PageSize);
                choice_ = -1;
            }
        }
        click_ = click;
        const double x = i.stick[hand_][0], y = i.stick[hand_][1];
        if (x * x + y * y >= .55 * .55)
        {
            const double pi = 3.14159265358979323846;
            double a = atan2(x, y);
            if (a < 0)
                a += 2 * pi;
            unsigned sector = (unsigned)floor(a * rows() / (2 * pi) + .5) % rows();
            choice_ = (int)(page_ * PageSize + sector);
            tapCandidate_ = false;
        }
        return out;
    }
};
}
