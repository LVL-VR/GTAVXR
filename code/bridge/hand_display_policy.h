#pragma once
// Pure display admission/pose, no target resolver or native state access.
#include "bridge.h"
#include <float.h>
#include <math.h>
namespace gx_hand_display {
inline bool poseValid(const GxPose&p){
    const double values[7]={p.x,p.y,p.z,p.qx,p.qy,p.qz,p.qw};
    for(unsigned i=0;i<7;++i)if(!_finite(values[i])||fabs(values[i])>100)return false;
    const double n=p.qx*p.qx+p.qy*p.qy+p.qz*p.qz+p.qw*p.qw;return fabs(n-1)<.01;
}
inline bool fresh(unsigned hand,uint64_t captured,uint64_t poseFrame,uint64_t frame,
                  unsigned validMask,const GxPose&grip,const GxPose&head){
    return hand<2&&frame&&captured==frame&&poseFrame==frame&&(validMask&(1u<<hand))&&
        poseValid(grip)&&poseValid(head);
}
inline GxPose nearHand(unsigned hand,const GxPose&grip,const GxPose&head){
    // A small hand-local cue IN FRONT of the fingers, not a new watch region.
    // Billboard orientation is the same-frame HMD orientation; no extra locate.
    const double side=hand?.08:-.08,up=.06,forward=-.12;
    const double x=grip.qx,y=grip.qy,z=grip.qz,w=grip.qw;
    GxPose p=head;
    p.x=grip.x+side*(1-2*(y*y+z*z))+up*2*(x*y-w*z)+forward*2*(x*z+w*y);
    p.y=grip.y+side*2*(x*y+w*z)+up*(1-2*(x*x+z*z))+forward*2*(y*z-w*x);
    p.z=grip.z+side*2*(x*z-w*y)+up*2*(y*z+w*x)+forward*(1-2*(x*x+y*y));
    return p;
}
}

