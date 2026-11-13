#pragma once
// Bridge-private fence; no field/export/layout changes to C's 32-byte v2 ABI.
#include "haptics.h"
namespace gx_haptic {
struct ImmediateOwnerGate {
    uint64_t readSerial,generalSerial,spatialSerial,inputTick,inputSequence;
    unsigned inputHandMask;
    float issuedHeavy,issuedLight;
    ImmediateOwnerGate():readSerial(0),generalSerial(0),spatialSerial(0),inputTick(0),inputSequence(0),inputHandMask(0),issuedHeavy(0),issuedLight(0){}
    void input(uint64_t now,float heavy,float light){
        if(++readSerial==0)++readSerial;
        generalSerial=0;inputTick=now;inputSequence=0;inputHandMask=0;issuedHeavy=heavy;issuedLight=light;
    }
    void owners(const GxInput&out){
        inputSequence=out.size==sizeof(out)&&out.version==1&&out.head_valid?out.sequence:0;
        inputHandMask=inputSequence?(out.active_hands&3u):0;
    }
    void general(){generalSerial=readSerial;}
    bool take(const GxRumble&spatial,const GxRumble&general,uint64_t now){
        if(!readSerial||generalSerial!=readSerial||spatialSerial==readSerial||!inputTick||now<inputTick||now-inputTick>100||
           spatial.version!=2||!valid(spatial)||general.version!=1||!valid(general)||
           !inputSequence||spatial.sequence!=inputSequence||spatial.heavy!=general.heavy||spatial.light!=general.light||
           (gx_haptic::spatial(spatial,0)&&!(inputHandMask&1u))||(gx_haptic::spatial(spatial,1)&&!(inputHandMask&2u))||
           (!gx_haptic::spatial(spatial,0)&&!gx_haptic::spatial(spatial,1)))return false;
        spatialSerial=readSerial;return true;
    }
};
}
