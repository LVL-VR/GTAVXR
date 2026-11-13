#pragma once
#include "frame.h"
namespace native_xr {
 
// distances. Task086's 120deg/1mm debug envelope limits are not engine limits.
// Keep the old envelope exactly when it fits; otherwise fit the actual two eyes
// with independent horizontal/vertical extents. No eye pose/projection is changed.
inline shared086::Envelope native_envelope(const eye036::Matrix&center,
    const std::array<pose075::FrameEye,2>&eyes,shared086::Clip clip,double aspect){
    auto old=shared086::envelope(center,eyes,clip,aspect);if(old.representable)return old;
    auto view=eye036::inverse(center);double tx=0,ty=0,minz=1e100,maxz=0;
    for(const auto&e:eyes){auto relative=eye036::mul(e.vi,view);const auto&p=e.projection;
        for(double z:{clip.near_z,clip.far_z})for(double y:{-1.,1.})for(double x:{-1.,1.}){
            const auto v=shared086::point({z*(x+p[8])/p[0],z*(y+p[9])/p[5],-z},relative);
            const double forward=-v[2];if(!std::isfinite(forward)||forward<=0)return old;
            tx=std::max(tx,std::abs(v[0])/forward);ty=std::max(ty,std::abs(v[1])/forward);
            minz=std::min(minz,forward);maxz=std::max(maxz,forward);
        }
    }
    ty*=1.001;tx*=1.001;
    shared086::Envelope result{2*atan(ty)*180/std::acos(-1.),minz*.999,maxz*1.001,tx/ty,false};
    result.representable=std::isfinite(result.aspect)&&result.aspect>0&&std::isfinite(result.vertical_degrees)&&
        result.vertical_degrees>0&&result.vertical_degrees<179&&result.near_z>0&&result.far_z>result.near_z;
    return result;
}
inline bool transient_compose_error(const char*error){
    return error&&(!strcmp(error,"CENTER_FRUSTUM_UNREPRESENTABLE")||!strcmp(error,"EYE_CLIP_POLICY")||!strcmp(error,"XR_POSE_REJECTED"));
}
}
