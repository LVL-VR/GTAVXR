// Host test for psvr2_input.h:  g++ -std=c++17 -I.. psvr2_input_test.cpp -o t && ./t
#include "../psvr2_input.h"
#include <cstdio>
#include <cstring>
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static GxInput in(uint32_t buttons, float lx = 0, float ly = 0, float rx = 0, float ry = 0) {
    GxInput i{}; i.size = sizeof(i); i.version = 1; i.active_hands = 3; i.buttons = buttons;
    i.stick[0][0] = lx; i.stick[0][1] = ly; i.stick[1][0] = rx; i.stick[1][1] = ry; return i;
}
static unsigned clicks(uint32_t b) { return ((b & GX_LCLICK) ? 1u : 0u) | ((b & GX_RCLICK) ? 2u : 0u); }
static GxInput step(gx_psvr2::Remap& m, bool psvr2, bool mod, GxInput i) { m.apply(i, psvr2, mod, clicks(i.buttons)); return i; }

int main() {
    // Other headsets: every button/stick combination passes through untouched, even with the modifier
    // action reporting true (it is never read off-PSVR2, but the policy must not depend on that).
    {
        gx_psvr2::Remap m;
        const uint32_t all[] = {0, GX_A, GX_B, GX_X, GX_Y, GX_LCLICK, GX_RCLICK, GX_MENU, GX_LCLICK | GX_RCLICK,
                                GX_MENU | GX_X, GX_RCLICK | GX_MENU, GX_A | GX_B | GX_X | GX_Y | GX_LCLICK | GX_RCLICK | GX_MENU};
        for (uint32_t b : all) for (int mod = 0; mod < 2; ++mod) {
            GxInput i = in(b, .3f, -.7f, .9f, -.2f), o = step(m, false, mod != 0, i);
            CHECK(o.buttons == i.buttons);
            CHECK(std::memcmp(o.stick, i.stick, sizeof i.stick) == 0);
            CHECK(((o.reserved & GX_INPUT_BOTH_CLICKS) != 0) == ((b & (GX_LCLICK | GX_RCLICK)) == (GX_LCLICK | GX_RCLICK)));
        }
        CHECK(!m.menuSeen && !m.shiftSeen);
    }
    // PSVR2 without the modifier: ordinary input unchanged.
    {
        gx_psvr2::Remap m;
        GxInput o = step(m, true, false, in(GX_A | GX_RCLICK, .5f, .5f, .8f, 0));
        CHECK(o.buttons == (GX_A | GX_RCLICK) && o.stick[1][0] == .8f && !(o.reserved & GX_INPUT_BOTH_CLICKS));
        o = step(m, true, false, in(GX_LCLICK | GX_RCLICK));
        CHECK(o.buttons == (GX_LCLICK | GX_RCLICK) && (o.reserved & GX_INPUT_BOTH_CLICKS)); // real L3+R3 menu chord
    }
    // PSVR2 Create (menu action): replayed as L3+R3 with centred sticks while held; release clears it.
    {
        gx_psvr2::Remap m;
        GxInput o = step(m, true, false, in(GX_MENU, .4f, .4f, .6f, .6f));
        CHECK(o.buttons == (GX_LCLICK | GX_RCLICK) && (o.reserved & GX_INPUT_BOTH_CLICKS));
        CHECK(o.stick[0][0] == 0 && o.stick[0][1] == 0 && o.stick[1][0] == 0 && o.stick[1][1] == 0);
        o = step(m, true, false, in(0));
        CHECK(o.buttons == 0 && !(o.reserved & GX_INPUT_BOTH_CLICKS));
    }
    // PSVR2 modifier + R3 with centred stick: menu chord, logged once; releasing R3 ends it.
    {
        gx_psvr2::Remap m;
        GxInput o = step(m, true, true, in(0));
        CHECK(o.buttons == 0);
        o = step(m, true, true, in(GX_RCLICK));
        CHECK(o.buttons == (GX_LCLICK | GX_RCLICK) && (o.reserved & GX_INPUT_BOTH_CLICKS) && m.menuSeen);
        o = step(m, true, true, in(0));
        CHECK(o.buttons == 0);
    }
    // R3 held before the modifier stays R3.
    {
        gx_psvr2::Remap m;
        step(m, true, false, in(GX_RCLICK));
        GxInput o = step(m, true, true, in(GX_RCLICK));
        CHECK(o.buttons == GX_RCLICK && !m.menuSeen);
    }
    // PSVR2 modifier + right stick: zeroed below 0.5, then L3+R3 held with the stick passed through, without
    // the both-clicks menu flag, until the modifier lifts; centring the stick mid-hold keeps the latch.
    {
        gx_psvr2::Remap m;
        GxInput o = step(m, true, true, in(GX_A, 0, 0, .3f, 0));
        CHECK(o.stick[1][0] == 0 && o.buttons == GX_A); // no turn while selecting
        o = step(m, true, true, in(0, 0, 0, 0, .9f));
        CHECK(o.buttons == (GX_LCLICK | GX_RCLICK) && o.stick[1][1] == .9f && m.shiftSeen);
        CHECK(!(o.reserved & GX_INPUT_BOTH_CLICKS));
        o = step(m, true, true, in(0));
        CHECK(o.buttons == (GX_LCLICK | GX_RCLICK) && !(o.reserved & GX_INPUT_BOTH_CLICKS));
        o = step(m, true, true, in(GX_RCLICK, -.8f, 0, -.9f, 0)); // physical R3 during the shift is not menu
        CHECK(o.buttons == (GX_LCLICK | GX_RCLICK) && o.stick[1][0] == -.9f && o.stick[0][0] == -.8f);
        o = step(m, true, false, in(0, 0, 0, .9f, 0));
        CHECK(o.buttons == 0 && o.stick[1][0] == .9f); // modifier lifted: turning back
    }
    // Create during an active D-pad shift does not open the menu.
    {
        gx_psvr2::Remap m;
        step(m, true, true, in(0, 0, 0, .9f, 0));
        GxInput o = step(m, true, true, in(GX_MENU, 0, 0, .9f, 0));
        CHECK(o.buttons == (GX_LCLICK | GX_RCLICK));
        CHECK(!(o.reserved & GX_INPUT_BOTH_CLICKS));
    }
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures != 0;
}
