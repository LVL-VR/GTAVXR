#pragma once
// The shared script DRAW_MENU producer refreshes THEFEED_SET_SCRIPTED_MENU_HEIGHT
// after its native text/sprite queue. No script names or localized labels here.
#include "../xr/locomotion.h"
namespace gx_script_menu
{
inline bool fresh(uint64_t tick, uint64_t now)
{
    return tick && now >= tick && now - tick <= 200;
}
inline bool height(float h)
{
    return gx_loco::finite(h) && h > .01f && h <= 1.5f;
}
struct Input
{
    bool owned, cursor;
    float up, down, left, right, accept, back, x, y, extraX, extraY, lt, rt, cursorX, cursorY;
    Input()
        : owned(false), cursor(false), up(0), down(0), left(0), right(0), accept(0), back(0), x(0), y(0),
          extraX(0), extraY(0), lt(0), rt(0), cursorX(.5f), cursorY(.5f)
    {
    }
};
class Ownership
{
    bool barrier, previousActive, entryDrain;
    double lastSeconds;
    float cursorX, cursorY;

  public:
    Ownership()
        : barrier(false), previousActive(false), entryDrain(false), lastSeconds(0), cursorX(.5f), cursorY(.5f)
    {
    }
    Input update(const GxInput& i, bool active, bool eligible, bool cursorMode = false, double seconds = 0)
    {
        Input o;
        if (!eligible)
        {
            barrier = previousActive = entryDrain = false;
            lastSeconds = 0;
            return o;
        }
        const double dt =
            lastSeconds && seconds >= lastSeconds ? gx_loco::clamp(seconds - lastSeconds, 0, .05) : 0;
        lastSeconds = seconds;
        const bool tracked = (i.active_hands & 2) != 0;
        const bool neutral = !tracked || (!(i.buttons & (GX_A | GX_B | GX_X | GX_Y | GX_RCLICK)) &&
                                          fabs(i.stick[1][0]) < .25 && fabs(i.stick[1][1]) < .25 &&
                                          i.trigger[0] < .2 && i.trigger[1] < .2 && i.grip[1] < .2);
        if (active && !previousActive)
        {
            entryDrain = !neutral;
            cursorX = cursorY = .5f;
        }
        previousActive = active;
        if (active)
            barrier = true;
        o.owned = active || barrier;
        if (!active)
        {
            if (neutral)
                barrier = false;
            return o;
        }
        if (entryDrain)
        {
            if (neutral)
                entryDrain = false;
            return o;
        }
        if (!tracked)
            return o;
        o.x = (float)gx_loco::clamp(i.stick[1][0], -1, 1);
        o.y = (float)gx_loco::clamp(-i.stick[1][1], -1, 1);
        o.up = i.stick[1][1] > .55;
        o.down = i.stick[1][1] < -.55;
        o.left = i.stick[1][0] < -.55;
        o.right = i.stick[1][0] > .55;
        o.accept = (i.buttons & GX_A) != 0;
        o.back = (i.buttons & GX_B) != 0;
        o.extraX = (i.buttons & GX_X) != 0;
        o.extraY = (i.buttons & GX_Y) != 0;
        o.lt = (float)gx_loco::clamp(i.trigger[0], 0, 1);
        o.rt = (float)gx_loco::clamp(i.trigger[1], 0, 1);
        o.cursor = cursorMode;
        if (cursorMode)
        {
            const float sx = fabs(o.x) > .15f ? o.x : 0, sy = fabs(o.y) > .15f ? o.y : 0;
            cursorX = (float)gx_loco::clamp(cursorX + sx * dt * .8, 0, 1);
            cursorY = (float)gx_loco::clamp(cursorY + sy * dt * .8, 0, 1);
        }
        o.cursorX = cursorX;
        o.cursorY = cursorY;
        return o;
    }
    static void filter(GxInput& i, bool owned)
    {
        if (!owned)
            return;
        i.buttons &= ~(GX_A | GX_B | GX_X | GX_Y | GX_RCLICK);
        i.stick[1][0] = i.stick[1][1] = 0;
        i.trigger[0] = i.trigger[1] = i.grip[1] = 0; // left stick and tracking stay active.
    }
};
}
