#pragma once
#include "input.h"
#include <math.h>
#include <string.h>

// L3 + R3 arbiter. It runs before either click reaches its own action.
// SteamVR keeps its accepted behaviour: the left Menu button belongs to the dashboard there, so a
// neutral chord opens the VR menu on release. Every other runtime keeps its Menu button and uses the
// same neutral chord to recenter. In both cases a tilted right stick hands the chord to the D-pad.
namespace gx_steam_menu
{
// A buffered single click reaches the game late. `deferred` names that click and `pressedAt` its physical
// press time. When the click was also released while buffered, `tapped` names it and `releasedAt` is the
// physical release time: the router then sees the whole tap in this update, so a new press in the next
// sample is a second click and not the same one. A tapped click is a replay, no longer held: gameButtons
// carries it only alone, and consumers never combine it with physical buttons into a chord.
struct Route
{
    unsigned menuButtons, gameButtons, suppressed, cancelled;
    bool consume, recenter;
    unsigned deferred;
    uint64_t pressedAt;
    unsigned tapped;
    uint64_t releasedAt;
};

// The buttons physically held in this update: the game buttons without a replayed tap.
inline unsigned physical(const Route& route) { return route.gameButtons & ~route.tapped; }

// The buttons the action router receives: the game buttons plus the replayed tap, which the router times with
// pressedAt / releasedAt. gameButtons carries the replay only when nothing else is physically held with it.
inline unsigned routed(const Route& route) { return route.gameButtons | route.tapped; }

// The keyboard key that opens the VR menu in SteamVR, chosen in the Suite (GTAVXR_STEAMVR_MENU_KEY): F7, F9, F11 or
// off. A missing or unknown value keeps the default F9.
enum MenuKey { MENU_KEY_OFF, MENU_KEY_F7, MENU_KEY_F9, MENU_KEY_F11 };
inline MenuKey menuKey(const char* value)
{
    if (!value)
        return MENU_KEY_F9;
    if (!strcmp(value, "off"))
        return MENU_KEY_OFF;
    if (!strcmp(value, "F7"))
        return MENU_KEY_F7;
    if (!strcmp(value, "F11"))
        return MENU_KEY_F11;
    return MENU_KEY_F9;
}

class Gesture
{
    static const unsigned clicks = GX_LCLICK | GX_RCLICK;
    enum State { WAIT_NEUTRAL, IDLE, FIRST, SINGLE, CHORD, DPAD, DRAIN };
    State state_;
    unsigned first_, carry_;
    uint64_t began_, carryAt_;
    bool enabled_, steam_;
    // Late clicks: a click pressed while the other click has been held for more than 100 ms is no chord and no
    // pair. It stays out of every output (game, menu, character gestures, router) until it is released, so the
    // held click keeps its own action and the game never sees both clicks (the native D-pad modifier).
    unsigned held_, late_;
    uint64_t pressAt_[2];

    static unsigned index(unsigned click) { return click == GX_LCLICK ? 0u : 1u; }

    void track(unsigned held, uint64_t now)
    {
        const unsigned edges = held & ~held_;
        for (unsigned click = GX_LCLICK; click; click = click == GX_LCLICK ? GX_RCLICK : 0)
            if (edges & click)
                pressAt_[index(click)] = now;
        for (unsigned click = GX_LCLICK; click; click = click == GX_LCLICK ? GX_RCLICK : 0)
        {
            const unsigned other = clicks & ~click;
            if ((edges & click) && (held & other) && !(edges & other) && !(late_ & other) &&
                now - pressAt_[index(other)] > 100)
                late_ |= click;
        }
        late_ &= held;
        held_ = held;
    }

    Route finish(Route route) const
    {
        route.menuButtons &= ~late_;
        route.gameButtons &= ~late_;
        route.suppressed &= ~late_;
        route.cancelled &= ~late_;
        return route;
    }

    // Both clicks held with a stick away from neutral: the existing D-pad modifier owns them.
    static void pad(Route& route, bool steam)
    {
        if (steam)
            route.menuButtons &= ~clicks;
        route.cancelled = clicks;
    }

  public:
    Gesture()
        : state_(WAIT_NEUTRAL), first_(0), carry_(0), began_(0), carryAt_(0), enabled_(false), steam_(false), held_(0),
          late_(0)
    {
        pressAt_[0] = pressAt_[1] = 0;
    }

    // interfaceOwner: a menu, the phone or another interface holds the controller. Outside SteamVR it
    // cancels a chord in progress and lets single clicks through unchanged.
    Route update(const GxInput& input, bool live, bool paused, uint64_t now, bool interfaceOwner = false)
    {
        const bool steam = (input.reserved & GX_INPUT_STEAMVR) != 0;
        const bool enabled = (input.reserved & GX_INPUT_BOTH_CLICKS) != 0;
        const unsigned raw = input.buttons & ~(steam && enabled ? GX_MENU : 0);
        Route out = {raw, raw, 0, 0, false, false, 0, 0, 0, 0};
        // A click pressed in the sample that ended a buffered tap keeps its own press time.
        const unsigned carry = carry_;
        const uint64_t carryAt = carryAt_;
        carry_ = 0;
        // Without the arbiter (no both-click controller) nothing is late: the pair reaches the game as before.
        if (enabled)
            track(raw & clicks, now);
        else
            held_ = late_ = 0;
        if (!enabled || !live || paused || !input.head_valid || (input.active_hands & 3) != 3)
        {
            if ((steam_ && enabled_) || state_ == FIRST || state_ == CHORD || state_ == DPAD)
                suppress(out);
            state_ = WAIT_NEUTRAL;
            enabled_ = enabled;
            steam_ = steam;
            return finish(out);
        }
        if (!enabled_ || steam != steam_)
            state_ = WAIT_NEUTRAL;
        enabled_ = true;
        steam_ = steam;
        const unsigned pressed = raw & clicks;
        if (!steam && (interfaceOwner || (raw & GX_MENU)))
        {
            if (state_ == FIRST || state_ == CHORD)
            {
                suppress(out);
                state_ = pressed ? DRAIN : IDLE;
            }
            else if (state_ == WAIT_NEUTRAL || state_ == DRAIN)
            {
                if (!pressed)
                    state_ = IDLE;
                else
                    suppress(out);
            }
            else
                state_ = pressed ? SINGLE : IDLE;
            return finish(out);
        }
        const bool rightMoved = fabs(input.stick[1][0]) > .32 || fabs(input.stick[1][1]) > .32;
        const bool leftMoved = fabs(input.stick[0][0]) > .32 || fabs(input.stick[0][1]) > .32;
        const bool face = (raw & (GX_A | GX_B | GX_X | GX_Y)) != 0;
        // SteamVR drains a chord pressed while walking. Elsewhere that chord was already the D-pad
        // modifier, so it stays one.
        const bool moved = rightMoved || (!steam && leftMoved);
        switch (state_)
        {
        case WAIT_NEUTRAL:
        case DRAIN:
            if (!pressed)
                state_ = IDLE;
            else
                suppress(out);
            break;
        case IDLE:
        {
            // A click carried from the sample that ended a buffered tap and already released here: it is a
            // complete tap too, pressed at its own time and released now. As in FIRST, a click pressed in
            // this same sample waits one more sample, so the two never overlap as a chord.
            const unsigned done = carry & ~pressed;
            if (done)
            {
                out.menuButtons &= ~clicks;
                out.gameButtons &= ~clicks;
                tap(out, done, carryAt, now);
                next(out, pressed, now, face || leftMoved || rightMoved);
                break;
            }
            if (!pressed)
                break;
            // A carried click keeps its own press time: both clicks form a chord only when the two
            // presses are at most 100 ms apart, as in FIRST. A later second click is no chord: the first
            // click goes on as a single click and the later one is a late click (track / finish).
            const uint64_t from = (carry & pressed) && carryAt <= now ? carryAt : now;
            if (pressed == clicks && !face && now - from > 100)
            {
                state_ = SINGLE;
                out.deferred = carry & pressed;
                out.pressedAt = from;
                if (steam)
                    out.menuButtons &= ~clicks;
            }
            else if (pressed == clicks && !face)
                state_ = moved ? DPAD : (leftMoved ? DRAIN : CHORD);
            else if (!face && !leftMoved && !rightMoved)
            {
                state_ = FIRST;
                first_ = pressed;
                began_ = (carry & pressed) == pressed && carryAt <= now ? carryAt : now;
            }
            else
                state_ = SINGLE;
            if (state_ != SINGLE && state_ != DPAD)
                suppress(out, state_ == FIRST);
            if (state_ == DPAD)
                pad(out, steam);
            break;
        }
        case FIRST:
            if (pressed == clicks && !face && now - began_ <= 100)
            {
                state_ = moved ? DPAD : (leftMoved ? DRAIN : CHORD);
                if (state_ != DPAD)
                    suppress(out);
                else
                    pad(out, steam);
            }
            else if (!(pressed & first_))
            {
                // Released while buffered: the game receives this one-sample press, and the router
                // receives the press and the release at their physical times.
                state_ = IDLE;
                out.menuButtons &= ~clicks;
                out.gameButtons &= ~clicks;
                tap(out, first_, began_, now);
                next(out, pressed, now, face || leftMoved || rightMoved);
            }
            else if (pressed != first_ || face || leftMoved || rightMoved || now - began_ > 100)
            {
                state_ = SINGLE;
                out.menuButtons |= first_;
                out.gameButtons |= first_;
                out.deferred = first_;
                out.pressedAt = began_;
            }
            else
                suppress(out, true);
            break;
        case SINGLE:
            if (!pressed)
                state_ = IDLE;
            if (steam && pressed == clicks)
                out.menuButtons &= ~clicks;
            break;
        case CHORD:
            suppress(out);
            if (face || (steam && leftMoved) || (moved && pressed != clicks))
                state_ = DRAIN;
            else if (pressed == clicks && moved)
            {
                state_ = DPAD;
                out.gameButtons = raw;
                out.suppressed = 0;
                if (!steam)
                    out.menuButtons = raw;
            }
            else if (!pressed)
            {
                state_ = IDLE;
                if (steam)
                {
                    out.menuButtons |= GX_MENU;
                    out.consume = true;
                }
                else
                    out.recenter = true;
            }
            break;
        case DPAD:
            pad(out, steam);
            if (pressed != clicks)
            {
                state_ = pressed ? DRAIN : IDLE;
                suppress(out);
            }
            break;
        }
        // A replayed tap was released before the buttons pressed in this update. Next to another physically held
        // click or face button the game would see a chord the player never made (L3 + A context, the native
        // both-clicks D-pad), so only the router receives it then, through `tapped`.
        if (out.gameButtons & ~out.tapped & ~late_ & (clicks | GX_A | GX_B | GX_X | GX_Y))
            out.gameButtons &= ~out.tapped;
        return finish(out);
    }

    // A whole one-sample tap of `click`: the game and the menu see it in this update, the router sees its
    // physical press and release times. It is never buffered or cancelled in the same update.
    static void tap(Route& route, unsigned click, uint64_t pressedAt, uint64_t releasedAt)
    {
        route.menuButtons |= click;
        route.gameButtons |= click;
        route.deferred = route.tapped = click;
        route.pressedAt = pressedAt;
        route.releasedAt = releasedAt;
        route.suppressed &= ~click;
        route.cancelled &= ~click;
    }

    // The other click pressed in the sample that delivers a whole tap. Alone it waits one sample, with its
    // press time, so the replayed tap and it never overlap as a chord. With a face button or a tilted
    // stick it can never start a chord: it reaches the game now, with them, as a single click does from
    // IDLE. Consumers tell the replayed tap apart through Route::tapped.
    void next(Route& route, unsigned click, uint64_t now, bool single)
    {
        if (!click)
            return;
        if (single)
        {
            state_ = SINGLE;
            route.menuButtons |= click;
            route.gameButtons |= click;
            return;
        }
        route.suppressed = click;
        carry_ = click;
        carryAt_ = now;
    }

    static void suppress(Route& route, bool pending = false)
    {
        route.menuButtons &= ~clicks;
        route.gameButtons &= ~clicks;
        if (pending)
            route.suppressed = clicks;
        else
            route.cancelled = clicks;
    }
};
}
