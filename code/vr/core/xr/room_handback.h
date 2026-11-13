#pragma once
 
 
 
// carried by exactly the body movement of that interval; a one-update body jump with no native travel (a mission / mod
// teleport) is carried the same way. Position only: the yaw and the physical lean are kept. Native travel (climb, vault,
// parachute, rappel, airborne, swim, attached) is attributed by the room transport and never reaches here as a carry; the
// eye-to-body distance (head through a wall) is never read. Pure C++03, no game headers.
#include "locomotion.h"
namespace gx_handback
{
using gx_loco::Vec2;
enum Reason
{
    NONE = 0,
    HANDBACK = 1,
    JUMP = 2
};
static const double MIN_CARRY = .25; // m: a brief loss of control without movement keeps everything as it is
static const double JUMP_DISTANCE = 2.0; // m in one update: above any walk / sprint / room-follow step
struct Input
{
    bool owned, travel;
    Vec2 body, last,
        origin; // origin = the room world origin now (it may already follow part of the movement)
    bool hasLast;
    unsigned
        generation; // room transport origin generation: a re-seed elsewhere (respawn, trainer teleport) cancels a carry
    // build169 item 1: the body posted after the PREVIOUS frame's physics (absent when that post is older). A script that
    // moves the player and takes control in the same tick (Open World Interiors exit) moved him after that post.
    Vec2 freshLast;
    bool hasFreshLast;
    Input() : owned(false), travel(false), hasLast(false), generation(0), hasFreshLast(false) {}
};
struct Output
{
    Reason reason;
    Vec2 carry;
    double distance;
    Output() : reason(NONE), distance(0) {}
};
inline double length(Vec2 a)
{
    return sqrt(a.x * a.x + a.y * a.y);
}
inline bool finite2(Vec2 a)
{
    return gx_loco::finite(a.x) && gx_loco::finite(a.y);
}
class Policy
{
    bool inside_;
    Vec2 start_, originStart_;
    unsigned generation_;

  public:
    Policy() : inside_(false), generation_(0) {}
    // Respawn / trainer teleport / vehicle / identity change: a fresh origin was set elsewhere.
    void reset()
    {
        inside_ = false;
    }
    bool inside() const
    {
        return inside_;
    }
    Output update(const Input& in)
    {
        Output out;
        if (!finite2(in.body))
        {
            inside_ = false;
            return out;
        }
        if (in.owned)
        {
            if (!inside_)
            {
                inside_ = true;
                originStart_ = in.origin;
                generation_ = in.generation;
                // build169 item 1: start where physics left the body, so a move made in the same script tick that took
                // control is carried at hand-back (168 started from the moved body: carry 0 m, camera left behind)
                start_ = in.hasFreshLast && finite2(in.freshLast) ? in.freshLast : in.body;
            }
            return out;
        }
        if (inside_)
        {
            inside_ = false;
            if (in.generation != generation_)
                return out; // the origin was re-seeded on the body meanwhile
            // only the movement nobody attributed: body movement minus what the origin already followed
            const Vec2 d((in.body.x - start_.x) - (in.origin.x - originStart_.x),
                         (in.body.y - start_.y) - (in.origin.y - originStart_.y));
            const double n = length(d);
            if (gx_loco::finite(n) && n >= MIN_CARRY)
            {
                out.reason = HANDBACK;
                out.carry = d;
                out.distance = n;
            }
            return out;
        }
        if (!in.travel && in.hasLast && finite2(in.last))
        {
            const Vec2 d(in.body.x - in.last.x, in.body.y - in.last.y);
            const double n = length(d);
            if (gx_loco::finite(n) && n >= JUMP_DISTANCE)
            {
                out.reason = JUMP;
                out.carry = d;
                out.distance = n;
            }
        }
        return out;
    }
};
}
