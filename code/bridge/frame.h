#pragma once
// GTAVXR-owned shared-view math. Source pose is NEVER the independent reference pose.
#include "reference.h"
#include <limits>
namespace shared086 {
using eye036::Matrix;using pose075::Pose;using pose075::Fov;using reference084::Vec;
struct Clip {double near_z=.1,far_z=1000;};
struct Reference {Matrix gameplay_vi;Pose xr_recenter;};
struct Source {Matrix consumed_view,consumed_projection;};
struct Envelope {double vertical_degrees{},near_z{},far_z{},aspect{};bool representable=false;};
struct Frame {Matrix base,center_vi;std::array<pose075::FrameEye,2>eyes;Envelope source_frustum;std::array<double,3>native_rotation;};
inline Vec point(Vec v,const Matrix&m){Vec out{};for(unsigned j=0;j<3;++j)out[j]=v[0]*m[j]+v[1]*m[4+j]+v[2]*m[8+j]+m[12+j];return out;}
inline Matrix clip_projection(Clip c){
 pose075::demand(std::isfinite(c.near_z)&&std::isfinite(c.far_z)&&c.near_z>=.001&&c.near_z<=1&&c.far_z>c.near_z&&c.far_z<=100000,"EYE_CLIP_POLICY");
 Matrix p{};p[0]=p[5]=1;p[10]=c.near_z/(c.far_z-c.near_z);p[11]=-1;p[14]=c.far_z*c.near_z/(c.far_z-c.near_z);return p;
}
inline Pose midpoint(Pose a,Pose b){
 pose075::pose(a);pose075::pose(b);
 double d=a.qx*b.qx+a.qy*b.qy+a.qz*b.qz+a.qw*b.qw;
 if(d<0){b.qx=-b.qx;b.qy=-b.qy;b.qz=-b.qz;b.qw=-b.qw;}
 Pose c{(a.x+b.x)/2,(a.y+b.y)/2,(a.z+b.z)/2,a.qx+b.qx,a.qy+b.qy,a.qz+b.qz,a.qw+b.qw};
 double n=std::sqrt(c.qx*c.qx+c.qy*c.qy+c.qz*c.qz+c.qw*c.qw);
 pose075::demand(n>1e-6,"EYE_ORIENTATION_PAIR");c.qx/=n;c.qy/=n;c.qz/=n;c.qw/=n;return c;
}
inline Matrix native_vi(const std::array<double,3>&degrees,Vec position){
 const double rad=std::acos(-1.)/180,x=degrees[0]*rad,y=degrees[1]*rad,z=degrees[2]*rad;
 const double cx=cos(x),sx=sin(x),cy=cos(y),sy=sin(y),cz=cos(z),sz=sin(z);
  
 return {cy*cz-sy*sx*sz,cy*sz+sy*sx*cz,-sy*cx,0,
         sy*cz+cy*sx*sz,sy*sz-cy*sx*cz,cy*cx,0,
         cx*sz,-cx*cz,-sx,0,position[0],position[1],position[2],1};
}
inline std::array<double,3> native_angles(const Matrix&vi,double pole_heading){
 reference084::rigid(vi);const double deg=180/std::acos(-1.);
 double x=asin(std::clamp(-vi[10],-1.,1.)),y,z;
 if(std::hypot(vi[8],vi[9])>1e-7){z=atan2(vi[8],-vi[9]);y=atan2(-vi[2],vi[6]);}
 else{z=pole_heading/deg;const double combined=atan2(vi[1],vi[0]);y=(x>=0?1.:-1.)*(combined-z);}
 auto result=std::array<double,3>{x*deg,y*deg,z*deg};
 auto rebuilt=native_vi(result,{vi[12],vi[13],vi[14]});
 for(unsigned i=0;i<16;++i)pose075::demand(std::abs(rebuilt[i]-vi[i])<2e-7,"NATIVE_EULER_RECONSTRUCTION");
 return result;
}
inline Envelope envelope(const Matrix&center,const std::array<pose075::FrameEye,2>&eyes,Clip clip,double aspect){
 pose075::demand(std::isfinite(aspect)&&aspect>=.5&&aspect<=4,"SOURCE_ASPECT");
 auto center_view=eye036::inverse(center);double tangent=0,min_z=std::numeric_limits<double>::infinity(),max_z=0;
 for(auto&e:eyes){
  auto eye_to_center=eye036::mul(e.vi,center_view);const auto&p=e.projection;
  for(double distance:{clip.near_z,clip.far_z})for(double y:{-1.,1.})for(double x:{-1.,1.}){
   Vec v{distance*(x+p[8])/p[0],distance*(y+p[9])/p[5],-distance};
   auto vcenter=point(v,eye_to_center);double forward=-vcenter[2];
   if(!(forward>1e-6))return {0,0,0,aspect,false};
   min_z=std::min(min_z,forward);max_z=std::max(max_z,forward);
   tangent=std::max({tangent,std::abs(vcenter[1])/forward,std::abs(vcenter[0])/(forward*aspect)});
  }
 }
 // Small numeric padding, NOT a motion prediction margin or global culling disable.
 Envelope result{2*atan(tangent*1.001)*180/std::acos(-1.),min_z*.999,max_z*1.001,aspect,false};
 result.representable=result.vertical_degrees>=20&&result.vertical_degrees<=120&&result.near_z>=.001&&result.far_z<=100000;
 return result;
}
inline Frame make(const Reference&r,const Source&s,const std::array<Pose,2>&current,const std::array<Fov,2>&fov,Clip clip,double aspect){
 reference084::rigid(r.gameplay_vi);reference084::rigid(eye036::inverse(s.consumed_view));
 Frame out;out.base=reference084::level(r.gameplay_vi,{0,0,1});
 auto local_to_game=eye036::mul(eye036::inverse(reference084::xr_reference(r.xr_recenter)),out.base);
 out.center_vi=eye036::mul(pose075::pose(midpoint(current[0],current[1])),local_to_game);
 auto desired_clip=clip_projection(clip);
 for(unsigned i=0;i<2;++i){
  auto&e=out.eyes[i];e.vi=eye036::mul(pose075::pose(current[i]),local_to_game);e.view=eye036::inverse(e.vi);
  e.projection=pose075::projection(fov[i],desired_clip);
  e.delta=eye036::mul(eye036::inverse(s.consumed_view),e.view);
 }
 out.source_frustum=envelope(out.center_vi,out.eyes,clip,aspect);
 const double heading=atan2(out.base[1],out.base[0])*180/std::acos(-1.);
 out.native_rotation=native_angles(out.center_vi,heading);return out;
}
}
