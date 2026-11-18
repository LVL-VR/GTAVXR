#pragma once
#include "button_map.h"

namespace gx_action_bindings
{
enum Action { SPECIAL, COVER, STEALTH, PHONE, CAMERA, AIR_FIRE, AIR_CYCLE, COUNT };
enum Gesture { TAP, DOUBLE, HOLD, DOWN };
enum ContextMask { FOOT = 1, VEHICLE = 2, AIRCRAFT = 4 };
enum { BUTTONS = gx_button_map::SOURCE_COUNT, DOUBLE_MS = 300, HOLD_MS = 500 };

struct Binding
{
    unsigned button;
    Gesture gesture;

    int code() const { return 1 + static_cast<int>(button) + static_cast<int>(gesture) * BUTTONS; }
    unsigned bit() const { return gx_button_map::sourceBit(button); }
};

inline Binding defaults(unsigned action)
{
    const Binding entries[COUNT] = {
        {gx_button_map::SRC_LCLICK, DOUBLE}, {gx_button_map::SRC_LCLICK, HOLD},
        {gx_button_map::SRC_LCLICK, TAP}, {gx_button_map::SRC_RCLICK, TAP},
        {gx_button_map::SRC_RCLICK, DOUBLE}, {gx_button_map::SRC_A, DOWN}, {gx_button_map::SRC_X, DOWN}};
    return entries[action];
}

inline const char* key(unsigned action)
{
    const char* entries[COUNT] = {"special", "cover", "stealth", "phone", "vehicle_camera", "aircraft_fire", "aircraft_cycle"};
    return entries[action];
}

inline unsigned contexts(unsigned action)
{
    const unsigned entries[COUNT] = {FOOT | VEHICLE | AIRCRAFT, FOOT, FOOT, FOOT | VEHICLE | AIRCRAFT,
                                     VEHICLE | AIRCRAFT, AIRCRAFT, AIRCRAFT};
    return entries[action];
}

inline bool permits(unsigned action, Gesture gesture)
{
    if (action == SPECIAL || action == CAMERA)
        return gesture == TAP || gesture == DOUBLE;
    return gesture == defaults(action).gesture;
}

struct Config
{
    int codes[COUNT];

    Config() { reset(); }
    void reset()
    {
        for (unsigned action = 0; action < COUNT; ++action)
            codes[action] = 0;
    }
    bool same(const Config& other) const
    {
        for (unsigned action = 0; action < COUNT; ++action)
            if (codes[action] != other.codes[action])
                return false;
        return true;
    }
    Binding get(unsigned action) const
    {
        const int code = codes[action];
        if (code < 1 || code > 4 * BUTTONS)
            return defaults(action);
        const Gesture mode = static_cast<Gesture>((code - 1) / BUTTONS);
        const Binding value = {static_cast<unsigned>((code - 1) % BUTTONS), mode};
        return permits(action, mode) ? value : defaults(action);
    }
    bool conflicts(unsigned action, Binding candidate) const
    {
        for (unsigned other = 0; other < COUNT; ++other)
        {
            if (other == action || !(contexts(other) & contexts(action)))
                continue;
            const Binding current = get(other);
            if (current.button == candidate.button &&
                (current.gesture == candidate.gesture || current.gesture == DOWN || candidate.gesture == DOWN))
                return true;
        }
        return false;
    }
    void validate()
    {
        for (unsigned action = 0; action < COUNT; ++action)
        {
            const Binding current = get(action);
            codes[action] = current.code() == defaults(action).code() ? 0 : current.code();
            if (conflicts(action, current))
            {
                reset();
                return;
            }
        }
    }
    void cycle(unsigned action, int direction)
    {
        int candidate = get(action).code() - 1;
        for (int step = 0; step < 4 * BUTTONS; ++step)
        {
            candidate = (candidate + (direction > 0 ? 1 : 4 * BUTTONS - 1)) % (4 * BUTTONS);
            const Binding choice = {static_cast<unsigned>(candidate % BUTTONS), static_cast<Gesture>(candidate / BUTTONS)};
            if (permits(action, choice.gesture) && !conflicts(action, choice))
            {
                codes[action] = choice.code() == defaults(action).code() ? 0 : choice.code();
                return;
            }
        }
    }
};

inline unsigned trackedButtons(const GxInput& input)
{
    const unsigned left = (input.active_hands & 1) ? GX_X | GX_Y | GX_LCLICK | GX_MENU : 0;
    const unsigned right = (input.active_hands & 2) ? GX_A | GX_B | GX_RCLICK : 0;
    return input.buttons & (left | right);
}

struct Output
{
    unsigned active, consumed;
    Output() : active(0), consumed(0) {}
    bool has(unsigned action) const { return (active & (1u << action)) != 0; }
};

class Router
{
    struct Key
    {
        bool blocked, down, waiting, holding, suppress;
        uint64_t first;
        unsigned routes;

        Key() : blocked(true), down(false), waiting(false), holding(false), suppress(false), first(0), routes(0) {}
        void cancel(bool pressed) { *this = Key(); blocked = pressed; }
    };
    Key keys_[BUTTONS];
    Config last_;
    uint64_t tick_;

  public:
    Router() : tick_(0) {}
    void reset()
    {
        for (unsigned button = 0; button < BUTTONS; ++button)
            keys_[button] = Key();
        tick_ = 0;
    }

    Output update(const Config& config, unsigned buttons, unsigned available, uint64_t now, bool seated = false,
                  unsigned suppressed = 0, unsigned cancelled = 0, unsigned deferred = 0,
                  uint64_t pressedAt = 0, unsigned tapped = 0, uint64_t releasedAt = 0)
    {
        if (!last_.same(config) || now < tick_)
            reset();
        last_ = config;
        tick_ = now;

        unsigned routes[BUTTONS][4] = {};
        for (unsigned action = 0; action < COUNT; ++action)
        {
            if (!(available & (1u << action)))
                continue;
            const Binding b = config.get(action);
            routes[b.button][b.gesture] = 1u << action;
        }
        Output out;
        for (unsigned n = 0; n < BUTTONS; ++n)
        {
            Key& key = keys_[n];
            const unsigned bit = gx_button_map::sourceBit(n);
            bool pressed = (buttons & bit) != 0;
            const uint64_t edge = pressed && !key.down && (deferred & bit) && pressedAt <= now ? pressedAt : now;
            // A buffered tap: pressed at `edge` and released at `release`, both inside this update.
            const bool tap = pressed && !key.down && (tapped & bit) && edge <= releasedAt && releasedAt <= now;
            const uint64_t release = tap ? releasedAt : now;
            const unsigned* mode = routes[n];
            const unsigned assigned = mode[TAP] | mode[DOUBLE] | mode[HOLD] | mode[DOWN];
            if (cancelled & bit)
            {
                key.cancel(pressed);
                key.routes = assigned;
                continue;
            }
            if (suppressed & bit)
            {
                // A possible first click stays buffered by the menu arbiter.
                // Preserve completed double-click history until it is actually claimed.
                continue;
            }
            if (assigned != key.routes)
            {
                key.cancel(pressed);
                key.routes = assigned;
            }
            // The other stick click is a chord only while this click is held too: a released click that waits
            // for its double-click window keeps it when the other stick is clicked next. Stick-click chords are
            // made of physically held clicks only: a replayed tap neither forms one nor is cancelled by one. A
            // face button still cancels a pending L3 as before, whether that L3 is held, waiting or replayed.
            const unsigned held = buttons & ~tapped;
            const bool replay = (tapped & bit) != 0;
            const bool chord = !replay && (held & GX_LCLICK) &&
                               ((held & (GX_RCLICK | GX_A | GX_B)) || (bit == GX_RCLICK && key.down));
            const bool leftChord = bit == GX_LCLICK &&
                                   ((held & (GX_A | GX_B | GX_X | GX_Y)) ||
                                    (!replay && (pressed || key.down) && (held & GX_RCLICK)));
            if (!assigned || (buttons & GX_MENU) || chord || leftChord)
            {
                key.cancel(pressed);
                key.routes = assigned;
                continue;
            }
            // The default seated L3 retains its first press for the horn. The second
            // press and its release belong exclusively to the ability.
            const bool horn = seated && bit == GX_LCLICK && assigned == (1u << SPECIAL) &&
                              mode[DOUBLE] && !key.suppress && !key.blocked;
            if (pressed && !horn)
                out.consumed |= bit;
            if (key.blocked)
            {
                key.blocked = pressed && !tap;
                continue;
            }
            if (mode[DOWN])
            {
                if (pressed)
                    out.active |= mode[DOWN];
                continue;
            }
            if (key.waiting && !key.down && edge - key.first > DOUBLE_MS)
            {
                out.active |= mode[TAP];
                key.waiting = false;
            }
            if (pressed && !key.down)
            {
                if (key.waiting && edge - key.first <= DOUBLE_MS)
                {
                    out.active |= mode[DOUBLE];
                    out.consumed |= bit;
                    key.waiting = false;
                    key.holding = true;
                    key.suppress = true;
                }
                else
                {
                    key.first = edge;
                    key.holding = false;
                }
            }
            if (tap)
            {
                key.down = true;
                pressed = false;
            }
            // The observed hold lasts from the press edge to this update, or to the release when it is seen
            // here. A press and release with no sample between them still counts as held when it lasted.
            if ((pressed || key.down) && !key.holding && mode[HOLD] && release - key.first >= HOLD_MS)
            {
                out.active |= mode[HOLD];
                key.holding = true;
            }
            if (!pressed && key.down)
            {
                if (!key.holding)
                {
                    if (mode[DOUBLE] && release - key.first <= DOUBLE_MS)
                        key.waiting = true;
                    else
                        out.active |= mode[TAP];
                }
                key.holding = false;
                key.suppress = false;
            }
            key.down = pressed;
        }
        return out;
    }
};
}
