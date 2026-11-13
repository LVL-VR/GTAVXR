#pragma once
// Paired83 ABI: RIGHT elements + dedicated LEFT wanted calibration.
// Automatic defaults retain the accepted parent panel; Individual is explicit.
#include "wrist_calibration.h"
namespace gx_ui_element {
enum Kind {Money,Notifications,Phone,Help,Subtitles,Location,Status,Other,Wanted,Count,RightCount=Wanted};
}
#pragma pack(push,8)
struct GxUiElementProfile {uint32_t individual;int32_t mm[3],degrees[3],percent;};
struct GxUiElementProfiles {uint32_t size,version;GxUiElementProfile element[gx_ui_element::Count];int32_t menuPercent;};
#pragma pack(pop)
static_assert(sizeof(GxUiElementProfile)==32,"element calibration POD ABI");
static_assert(sizeof(GxUiElementProfiles)==300,"element profiles84 POD ABI");
typedef int (GX_CALL *GxSetUiElementProfiles)(void*,const GxUiElementProfiles*);
namespace gx_ui_element {
inline GxUiElementProfiles defaults(){GxUiElementProfiles p={};p.size=sizeof(p);p.version=3;p.menuPercent=100;
    for(unsigned k=0;k<Count;++k)p.element[k].percent=100;return p;}
inline bool valid(const GxUiElementProfiles&p){
    if(p.size!=sizeof(p)||p.version!=3||p.menuPercent<40||p.menuPercent>200)return false;
    for(unsigned k=0;k<Count;++k){const GxUiElementProfile&e=p.element[k];
        if(e.individual>1||e.percent<40||e.percent>200)return false;
        for(unsigned c=0;c<3;++c)if(e.mm[c]<-300||e.mm[c]>300||e.degrees[c]<-180||e.degrees[c]>180)return false;}
    return true;
}
inline bool same(const GxUiElementProfiles&a,const GxUiElementProfiles&b){return memcmp(&a,&b,sizeof(a))==0;}
inline void reset(GxUiElementProfiles&p,unsigned kind){if(kind<Count)p.element[kind]=defaults().element[kind];}
inline void resetAll(GxUiElementProfiles&p){p=defaults();}
inline void resetRight(GxUiElementProfiles&p){for(unsigned k=0;k<RightCount;++k)reset(p,k);}
// A parent's reset never resets this extension; an element reset never resets parent/LEFT.
inline GxPose pose(const GxPose&anchor,const GxUiElementProfile&p,float parentScale){
    const double radians=3.14159265358979323846/360;
    const gx_wrist::Quaternion q(anchor.qx,anchor.qy,anchor.qz,anchor.qw);
    const gx_wrist::Quaternion rx(sin(p.degrees[0]*radians),0,0,cos(p.degrees[0]*radians));
    const gx_wrist::Quaternion ry(0,sin(p.degrees[1]*radians),0,cos(p.degrees[1]*radians));
    const gx_wrist::Quaternion rz(0,0,sin(p.degrees[2]*radians),cos(p.degrees[2]*radians));
    const gx_wrist::Quaternion rotation=gx_wrist::multiply(q,gx_wrist::multiply(gx_wrist::multiply(rx,ry),rz));
    const gx_wrist::Quaternion translated=gx_wrist::multiply(gx_wrist::multiply(q,
        gx_wrist::Quaternion(p.mm[0]*.001*parentScale,p.mm[1]*.001*parentScale,p.mm[2]*.001*parentScale,0)),
        gx_wrist::Quaternion(-q.x,-q.y,-q.z,q.w));
    GxPose out=anchor;out.x+=translated.x;out.y+=translated.y;out.z+=translated.z;
    out.qx=rotation.x;out.qy=rotation.y;out.qz=rotation.z;out.qw=rotation.w;return out;
}
inline float width(unsigned kind,unsigned pixelsWide,unsigned pixelsHigh,const GxUiElementProfile&p,float parentScale){
    if(kind>=Count||!pixelsWide||!pixelsHigh)return 0;
    const float reference=kind==Money?.24f:(kind==Location?.28f:(kind==Phone||kind==Notifications||kind==Status?.32f:.40f));
    const float aspect=float(pixelsHigh)/pixelsWide;
    const float ownWidth=reference*aspect>.52f?.52f/aspect:reference;
    return ownWidth*(p.percent*.01f)*parentScale;
}
}
