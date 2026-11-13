#pragma once
// Copy native wanted pixels below the accepted map quad; never count stars here.
#include "wrist_calibration.h"
namespace gx_wanted_ui {
struct Row {double width,height,down;Row():width(0),height(0),down(0){} };
inline bool row(unsigned mapPixelsW,unsigned mapPixelsH,unsigned pixelsW,unsigned pixelsH,
                double mapWidth,double mapHeight,double leftScale,Row&out){
    if(!mapPixelsW||!mapPixelsH||!pixelsW||!pixelsH||!_finite(mapWidth)||!_finite(mapHeight)||
       !_finite(leftScale)||mapWidth<=0||mapHeight<=0||leftScale<=0)return false;
    // Equal pixel density where it fits; clamp ONLY the new row to map width.
    const double ratio=double(pixelsW)/mapPixelsW;
    Row value;value.width=mapWidth*(ratio<1?ratio:1);value.height=value.width*pixelsH/pixelsW;
    value.down=(mapHeight+value.height)*.5+.008*leftScale;
    if(!_finite(value.width)||!_finite(value.height)||!_finite(value.down))return false;
    out=value;return true;
}
inline GxPose below(const GxPose&map,double down){
    const gx_wrist::Quaternion q(map.qx,map.qy,map.qz,map.qw);
    const gx_wrist::Quaternion d=gx_wrist::multiply(gx_wrist::multiply(q,gx_wrist::Quaternion(0,-down,0,0)),
        gx_wrist::Quaternion(-q.x,-q.y,-q.z,q.w));
    GxPose out=map;out.x+=d.x;out.y+=d.y;out.z+=d.z;return out;
}
}
