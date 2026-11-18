#pragma once
// PSVR2 Sense through SteamVR (always the Oculus Touch profile). Pure post-processing of one GxInput read;
// with psvr2=false it only derives GX_INPUT_BOTH_CLICKS, exactly as before, so other headsets are unchanged.
//   modifier            = left Triangle touch (secondary_touch) or SteamVR's dpad_shift (thumbrest touch) binding.
//   modifier + R3       = menu. Bare R3 is unchanged; an R3 already held when the modifier starts stays R3.
//   modifier + R stick  = D-pad: L3+R3 is held so the game's own stick-click D-pad chord picks the direction.
//                         The clicks appear only past 0.5 deflection (never the centred-sticks menu chord) and
//                         stay until the modifier lifts. Below that the right stick is zeroed so turning cannot fire.
//   menu (Create, etc.) = on SteamVR the game ignores Menu and opens its VR menu on L3+R3 released with centred
//                         sticks, so menu is replayed as exactly that chord while held.
#include "input.h"
#include <cmath>
namespace gx_psvr2 {
struct Remap {
    bool r3Held=false,r3Menu=false,shiftLatch=false;
    bool menuSeen=false,shiftSeen=false; // one-time log lines
    void reset(){*this=Remap();}
    // clickHands: bit h = that hand's stick_click action is active and pressed.
    void apply(GxInput& out,bool psvr2,bool modifier,unsigned clickHands){
        if(!psvr2){r3Held=r3Menu=shiftLatch=false;if(clickHands==3)out.reserved|=GX_INPUT_BOTH_CLICKS;return;}
        const bool r3=(out.buttons&GX_RCLICK)!=0;
        if(r3&&!r3Held)r3Menu=modifier&&!shiftLatch;
        r3Held=r3;if(!r3)r3Menu=false;
        if(r3Menu){out.buttons=(out.buttons&~uint32_t(GX_RCLICK))|GX_MENU;clickHands&=~2u;menuSeen=true;}
        const float rx=out.stick[1][0],ry=out.stick[1][1];
        if(!modifier||r3Menu)shiftLatch=false;
        else if(!shiftLatch&&(std::fabs(rx)>.5f||std::fabs(ry)>.5f)){shiftLatch=true;shiftSeen=true;}
        if(modifier&&!r3Menu){
            if(shiftLatch){out.buttons|=GX_LCLICK|GX_RCLICK;clickHands=0;}
            else out.stick[1][0]=out.stick[1][1]=0;
        }
        if(clickHands==3)out.reserved|=GX_INPUT_BOTH_CLICKS;
        if(shiftLatch){out.buttons&=~uint32_t(GX_MENU);return;} // the D-pad shift owns the chord
        if(out.buttons&GX_MENU){
            out.buttons=(out.buttons&~uint32_t(GX_MENU))|GX_LCLICK|GX_RCLICK;out.reserved|=GX_INPUT_BOTH_CLICKS;
            out.stick[0][0]=out.stick[0][1]=out.stick[1][0]=out.stick[1][1]=0;
        }
    }
};
}
