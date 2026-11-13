#pragma once
#include "haptics.h"
namespace gx_haptic {
// Bridge-private observation of requested API commands, NOT hardware telemetry.
struct HapticPublication {
    struct Pulse {
        uint64_t issued,deadline;float amplitude;bool known;
        Pulse():issued(0),deadline(0),amplitude(0),known(true){}
    } hand[2];
    void reset(){hand[0]=Pulse();hand[1]=Pulse();}
    void applied(unsigned h,float amplitude,uint64_t issued){
        if(h>=2)return;
        if(issued>UINT64_MAX-50||!_finite(amplitude)||amplitude<=0||amplitude>1){failed(h);return;}
        hand[h].issued=issued;hand[h].deadline=issued+50;hand[h].amplitude=amplitude;hand[h].known=true;
    }
    void stopped(unsigned h){if(h<2)hand[h]=Pulse();}
    void failed(unsigned h){if(h<2)hand[h].known=false;} // preserve old observations; NEVER invent CLEAR
    bool known(unsigned h,uint64_t now)const{return h<2&&hand[h].known&&now>=hand[h].issued;}
    bool covers(unsigned h,float spatial,uint64_t now)const{
        return known(h,now)&&now<hand[h].deadline&&hand[h].amplitude>=spatial;
    }
};
}
