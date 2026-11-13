#pragma once
#include "hand_context.h"
// Semantic extension of GxUiItem, NOT a new POD size or a backward-compatible
// promise to an old DLL. Root builds a new bridge/candidate together.
namespace gx_hand_routes {
enum {Left=3,Right=4,Base=0x60000000,MaxWidth=1024,MaxHeight=384};
inline bool route(unsigned value){return value==Left||value==Right;}
inline unsigned hand(unsigned value){return value==Left?0:(value==Right?1:2);}
inline unsigned id(unsigned hand,gx_hand_context::Owner owner){
    return Base|((unsigned(owner)+1)<<4)|(hand+1);
}
inline bool ownerAllowed(unsigned hand,gx_hand_context::Owner owner){
    return hand<2&&((owner==gx_hand_context::FREE&&hand==1)||
        owner==gx_hand_context::SUPPORT||(owner==gx_hand_context::PHONE&&hand==1)||
        owner==gx_hand_context::VEHICLE||owner==gx_hand_context::PHYSICS);
}
inline bool valid(unsigned identity,unsigned value,unsigned width,unsigned height){
    const unsigned h=hand(value),code=(identity>>4)&15;
    if(h>=2||!code||code>unsigned(gx_hand_context::CONFLICT))return false;
    const gx_hand_context::Owner owner=(gx_hand_context::Owner)(code-1);
    return ownerAllowed(h,owner)&&identity==id(h,owner)&&width&&height&&width<=MaxWidth&&height<=MaxHeight;
}
inline bool contextId(unsigned identity){return (identity&0xffff0000u)==Base;}
}
