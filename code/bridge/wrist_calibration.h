#pragma once
// Independent, versioned owned extension; GxApi/GxInput/GxUiCapture stay unchanged.
#include "bridge.h"
#include <math.h>
#include <string.h>
#pragma pack(push,8)
struct GxWristProfile {int32_t mm[3],degrees[3],percent;};
struct GxWristProfiles {uint32_t size,version;GxWristProfile hand[2];};
#pragma pack(pop)
static_assert(sizeof(GxWristProfiles)==64,"wrist calibration POD ABI");
typedef int (GX_CALL *GxSetWristProfiles)(void*,const GxWristProfiles*);
namespace gx_wrist {
inline GxWristProfiles defaults(){GxWristProfiles p={};p.size=sizeof(p);p.version=1;p.hand[0].percent=p.hand[1].percent=100;return p;}
inline bool valid(const GxWristProfiles&p){
    if(p.size!=sizeof(p)||p.version!=1)return false;
    for(unsigned h=0;h<2;++h){if(p.hand[h].percent<40||p.hand[h].percent>200)return false;
        for(unsigned c=0;c<3;++c)if(p.hand[h].mm[c]<-300||p.hand[h].mm[c]>300||p.hand[h].degrees[c]<-180||p.hand[h].degrees[c]>180)return false;}
    return true;
}
inline bool same(const GxWristProfiles&a,const GxWristProfiles&b){return memcmp(&a,&b,sizeof(a))==0;}
inline void reset(GxWristProfiles&p,unsigned hand){if(hand<2)p.hand[hand]=defaults().hand[hand];}
struct Quaternion {double x,y,z,w;Quaternion(double a=0,double b=0,double c=0,double d=1):x(a),y(b),z(c),w(d){} };
inline Quaternion multiply(const Quaternion&a,const Quaternion&b){
    return Quaternion(a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
        a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
        a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z);
}
inline GxPose pose(const GxPose&grip,const GxWristProfile&p,unsigned hand){
    const double radians=3.14159265358979323846/360;
    const Quaternion q(grip.qx,grip.qy,grip.qz,grip.qw);
    const Quaternion rx(sin(p.degrees[0]*radians),0,0,cos(p.degrees[0]*radians));
    const Quaternion ry(0,sin(p.degrees[1]*radians),0,cos(p.degrees[1]*radians));
    const Quaternion rz(0,0,sin(p.degrees[2]*radians),cos(p.degrees[2]*radians));
    const double k=.7071067811865475244;
    // Rotation/translation edits are in grip-local axes; default is the accepted watch pose.
    const Quaternion rotation=multiply(q,multiply(multiply(multiply(rx,ry),rz),Quaternion(-k,0,0,k)));
    const double local[3]={p.mm[0]*.001,(hand?.07:.055)+p.mm[1]*.001,(hand?.13:.08)+p.mm[2]*.001};
    const Quaternion translated=multiply(multiply(q,Quaternion(local[0],local[1],local[2],0)),Quaternion(-q.x,-q.y,-q.z,q.w));
    GxPose out=grip;out.x+=translated.x;out.y+=translated.y;out.z+=translated.z;
    out.qx=rotation.x;out.qy=rotation.y;out.qz=rotation.z;out.qw=rotation.w;return out;
}
inline float scale(const GxWristProfile&p){return p.percent*.01f;}
}
