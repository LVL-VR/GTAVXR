#pragma once
namespace gx_travel
{
enum Reason
{
    NONE = 0,
    CLIMB = 1,
    VAULT = 2,
    PARACHUTE = 4,
    RAPPEL = 8,
    AIRBORNE = 16,
    SWIM = 32,
    ATTACHED = 64
};
struct State
{
    bool climb, vault, parachute, rappel, jumping, falling, landing, diving, swimming, attached;
    bool local, human, alive, ragdoll, vehicle, cinematic, paused, tracking, yawReady;
    State()
        : climb(false), vault(false), parachute(false), rappel(false), jumping(false), falling(false),
          landing(false), diving(false), swimming(false), attached(false), local(false), human(false),
          alive(false), ragdoll(false), vehicle(false), cinematic(false), paused(false), tracking(false),
          yawReady(false)
    {
    }
};
inline unsigned reasons(const State& s)
{
    if (!s.local || !s.human || !s.alive || s.ragdoll || s.vehicle || s.cinematic || s.paused ||
        !s.tracking || !s.yawReady)
        return NONE;
    return (s.climb ? CLIMB : 0) | (s.vault ? VAULT : 0) | (s.parachute ? PARACHUTE : 0) |
           (s.rappel ? RAPPEL : 0) | ((s.jumping || s.falling || s.landing || s.diving) ? AIRBORNE : 0) |
           (s.swimming ? SWIM : 0) | (s.attached ? ATTACHED : 0);
}
}
