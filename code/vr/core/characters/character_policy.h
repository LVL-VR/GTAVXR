#pragma once
// Quest gesture arbitration and finite native-selector REQUESTS, not a Story system.
#include "../xr/locomotion.h"
#include "../input/single_click_intent.h"
#include "../input/double_click_intent.h"
namespace gx_character
{
enum Kind
{
    NO_ACTION,
    NATIVE_CONTEXT,
    NATIVE_CONTEXT_SECONDARY,
    SPECIAL_ABILITY,
    STEALTH,
    COVER
};
struct ActionState
{
    Kind kind;
    unsigned input, handMask, identityEpoch;
    uint64_t sequence;
    bool canExecute, pressed, availabilityKnown;
    const char* name;
    const char* binding;
    ActionState()
        : kind(NO_ACTION), input(0), handMask(0), identityEpoch(0), sequence(0), canExecute(false),
          pressed(false), availabilityKnown(false), name(""), binding("")
    {
    }
};
struct Emissions
{
    bool reserveClicks, consumeLeft, special, context, secondary, stealth, cover;
    Emissions()
        : reserveClicks(false), consumeLeft(false), special(false), context(false), secondary(false),
          stealth(false), cover(false)
    {
    }
};
class Gestures
{
    bool barrier_, held_, moved_, context_, secondary_;
    uint64_t sequence_;
    DoubleClickIntent leftIntent_;
    bool leftWas_, holdArmed_, holdFired_;
    uint64_t leftDownAt_;

  public:
    // build132: L3 held HOLD_MS on foot = ONE native cover request. Only a first press arms it (never
    // the second press of a double), any face/R3/menu chord or seat disarms it, and a fired hold
    // cancels the pending single so its release never also toggles stealth. Short L3 = stealth and
    // the 300 ms double = ability are unchanged.
    enum
    {
        HOLD_MS = 500
    };
    Gestures()
    {
        reset();
    }
    void reset()
    {
        barrier_ = true;
        held_ = moved_ = context_ = secondary_ = false;
        sequence_ = 0;
        leftIntent_.reset();
        leftWas_ = holdArmed_ = holdFired_ = false;
        leftDownAt_ = 0;
    }
    Emissions update(const GxInput& i, bool eligible, bool seated = false, uint64_t nowMs = 0)
    {
        Emissions out;
        const unsigned clicks = i.buttons & (GX_LCLICK | GX_RCLICK);
        const bool both = clicks == (GX_LCLICK | GX_RCLICK);
        const bool released = clicks == 0;
        bool valid =
            i.size == sizeof(i) && i.version == 1 && (i.active_hands & 3) == 3 && i.head_valid && i.sequence;
        for (unsigned h = 0; h < 2; ++h)
            valid = valid && gx_loco::finite(i.stick[h][0]) && gx_loco::finite(i.stick[h][1]);
        if (!valid || !eligible)
        {
            reset();
            return out;
        }
        if (sequence_ && i.sequence < sequence_)
        {
            reset();
            return out;
        }
        const bool repeated = i.sequence == sequence_;
        sequence_ = i.sequence;
        const bool neutral = fabs(i.stick[0][0]) < .25 && fabs(i.stick[0][1]) < .25 &&
                             fabs(i.stick[1][0]) < .25 && fabs(i.stick[1][1]) < .25;
        const bool moved = fabs(i.stick[0][0]) > .45 || fabs(i.stick[0][1]) > .45 ||
                           fabs(i.stick[1][0]) > .45 || fabs(i.stick[1][1]) > .45;
        if (barrier_)
        {
            out.reserveClicks = !released;
            leftIntent_.update((clicks & GX_LCLICK) != 0, true, true, nowMs);
            leftWas_ = (clicks & GX_LCLICK) != 0;
            holdArmed_ = holdFired_ = false;
            if (released)
                barrier_ = false;
            return out;
        }
        const bool leftDown = (clicks & GX_LCLICK) != 0;
        const bool leftCancel = (i.buttons & (GX_RCLICK | GX_A | GX_B | GX_X | GX_Y | GX_MENU)) != 0 || held_;
        const bool rise = !repeated && leftDown && !leftWas_;
        if (!repeated)
            leftWas_ = leftDown;
        if (leftCancel || seated || !leftDown)
            holdArmed_ = false;
        if (holdArmed_ && nowMs >= leftDownAt_ && nowMs - leftDownAt_ >= HOLD_MS)
        {
            out.cover = true;
            holdFired_ = true;
            holdArmed_ = false;
        }
        out.consumeLeft = !seated && (leftDown || leftIntent_.pending());
        const DoubleClickIntent::Result intent =
            leftIntent_.update(leftDown, leftCancel || holdFired_, true, nowMs, !repeated);
        if (rise)
        {
            holdArmed_ = !seated && !leftCancel && !intent.doublePress && !intent.suppress;
            leftDownAt_ = nowMs;
        }
        if (!repeated && !leftDown)
            holdFired_ = false;
        out.special = intent.doublePress;
        out.stealth = intent.single && !seated;
        // First seated L3 retains its native held horn/context. Consume the
        // second rising edge only when it is the deliberate ability request.
        out.consumeLeft = out.consumeLeft || out.special || intent.suppress;
        if (repeated)
        {
            out.reserveClicks = held_;
            out.context = context_;
            out.secondary = secondary_;
            return out;
        }
        context_ = secondary_ = false;
        if (both && !held_)
        {
            held_ = true;
            moved_ = !neutral || (i.buttons & (GX_A | GX_B | GX_X | GX_Y | GX_MENU)) != 0;
        }
        if (held_)
        {
            moved_ = moved_ || moved || (i.buttons & (GX_A | GX_B | GX_X | GX_Y | GX_MENU)) != 0;
            out.reserveClicks = true;
            if (released)
            {
                held_ = moved_ = false;
            }
            return out;
        }
        // Existing Lclick+A is retained. Secondary is a disjoint native control;
        // neither chord may also send jump/back or toggle native stealth.
        if ((i.buttons & GX_LCLICK) && !(i.buttons & GX_RCLICK))
        {
            const bool primary = (i.buttons & GX_A) != 0, secondary = (i.buttons & GX_B) != 0;
            out.context = context_ = primary && !secondary;
            out.secondary = secondary_ = secondary && !primary;
        }
        return out;
    }
};
enum Category
{
    STORY_MODELS,
    AMBIENT_MALE,
    AMBIENT_FEMALE,
    SERVICE,
    GANG,
    MISSION,
    CUTSCENE,
    ONLINE_MODELS,
    ANIMALS,
    OTHER_MODELS,
    CATEGORY_COUNT
};
inline bool prefix(const char* name, const char* start)
{
    return name && strncmp(name, start, strlen(start)) == 0;
}
inline unsigned category(const char* name, bool animal)
{
    if (animal)
        return ANIMALS;
    if (prefix(name, "player_"))
        return STORY_MODELS;
    if (prefix(name, "a_m_"))
        return AMBIENT_MALE;
    if (prefix(name, "a_f_"))
        return AMBIENT_FEMALE;
    if (prefix(name, "s_"))
        return SERVICE;
    if (prefix(name, "g_"))
        return GANG;
    if (prefix(name, "ig_"))
        return MISSION;
    if (prefix(name, "cs_") || prefix(name, "csb_"))
        return CUTSCENE;
    if (prefix(name, "mp_"))
        return ONLINE_MODELS;
    return OTHER_MODELS;
}
enum RequestStage
{
    IDLE,
    HOLD_SELECTOR,
    AWAIT_NATIVE
};
enum RequestResult
{
    NO_RESULT,
    OBSERVED_NATIVE,
    REQUEST_CANCELLED,
    REQUEST_TIMEOUT
};
class StoryRequest
{
    RequestStage stage_;
    int target_;
    uint64_t began_, released_;
    unsigned gameBegan_;

  public:
    StoryRequest() : stage_(IDLE), target_(-1), began_(0), released_(0), gameBegan_(0) {}
    void cancel()
    {
        stage_ = IDLE;
        target_ = -1;
        began_ = released_ = 0;
        gameBegan_ = 0;
    }
    bool begin(int target, uint64_t now, unsigned gameTime)
    {
        if (stage_ != IDLE || target < 0 || target > 2 || !now)
            return false;
        target_ = target;
        began_ = now;
        gameBegan_ = gameTime;
        stage_ = HOLD_SELECTOR;
        return true;
    }
    bool busy() const
    {
        return stage_ != IDLE;
    }
    bool holding() const
    {
        return stage_ == HOLD_SELECTOR;
    }
    int target() const
    {
        return target_;
    }
    RequestStage stage() const
    {
        return stage_;
    }
    RequestResult update(uint64_t now, unsigned gameTime, bool allowed, bool switchActive,
                         int nativeCharacter, bool modelMatches)
    {
        if (stage_ == IDLE)
            return NO_RESULT;
        if (now < began_ || !allowed)
        {
            cancel();
            return REQUEST_CANCELLED;
        }
        if (stage_ == AWAIT_NATIVE && !switchActive && nativeCharacter == target_ && modelMatches)
        {
            cancel();
            return OBSERVED_NATIVE;
        }
        // Native selector thresholds are 200/300 GAME ms. 400 covers both;
        // bounded wall-time timeout avoids a stuck request under pause/timewarp.
        if (stage_ == HOLD_SELECTOR && unsigned(gameTime - gameBegan_) >= 400)
        {
            stage_ = AWAIT_NATIVE;
            released_ = now;
        }
        if ((stage_ == HOLD_SELECTOR && now - began_ >= 8000) ||
            (stage_ == AWAIT_NATIVE && now - released_ >= 30000))
        {
            cancel();
            return REQUEST_TIMEOUT;
        }
        return NO_RESULT;
    }
};
// Loaded assets must have native skinned drawable + shader + valid bone schema.
// This is structural admission, not a claim that every animal's VR IK is supported.
inline bool modelStanceAllowed(bool restore, bool human, bool inControl, bool onGround, bool swimming)
{
    // Recovery from our nonhuman model must not require human walking stance.
    // Native session/death/load/script/attachment guards remain outside this helper.
    return (restore && !human) || (inControl && onGround && !swimming);
}
inline const char* modelFailure(bool ped, bool loaded, bool drawable, bool shader, bool skeleton,
                                unsigned bones)
{
    if (!ped)
        return "NOT_REGISTERED_PED";
    if (!loaded)
        return "ASSETS_PENDING";
    if (!drawable)
        return "NO_NATIVE_DRAWABLE";
    if (!shader)
        return "NO_PED_SHADER";
    if (!skeleton || !bones || bones > 4096)
        return "NO_VALID_NATIVE_SKELETON";
    return 0;
}
}
