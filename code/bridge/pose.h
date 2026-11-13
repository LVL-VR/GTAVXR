#pragma once
#include "eye_matrices.h"
#include "consumer_pose.h"
#include <algorithm>
namespace pose075 {
using namespace eye036;
struct Pose {double x{},y{},z{},qx{},qy{},qz{},qw{1};};
struct Fov {double left{},right{},up{},down{};};
enum class Kind {Rigid,Skeletal815,Ped2381,VehicleCE6255};
inline void demand(bool value,const char*why){if(!value)throw std::runtime_error(why);}
inline Matrix pose(const Pose&p){
 const double values[]{p.x,p.y,p.z,p.qx,p.qy,p.qz,p.qw};for(auto v:values)demand(std::isfinite(v),"NONFINITE_XR_POSE");
 const auto x=p.qx,y=p.qy,z=p.qz,w=p.qw;demand(std::abs(x*x+y*y+z*z+w*w-1)<1e-5,"XR_QUATERNION_NORM");
 return {1-2*(y*y+z*z),2*(x*y+z*w),2*(x*z-y*w),0,
  2*(x*y-z*w),1-2*(x*x+z*z),2*(y*z+x*w),0,
  2*(x*z+y*w),2*(y*z-x*w),1-2*(x*x+y*y),0,p.x,p.y,p.z,1};
}
inline Matrix projection(const Fov&f,const Matrix&native){
 for(auto x:{f.left,f.right,f.up,f.down})demand(std::isfinite(x)&&std::abs(x)<1.56,"XR_FOV_FINITE_BOUND");
 demand(f.left<f.right&&f.down<f.up,"XR_FOV_ORDER");
 demand(std::isfinite(native[10])&&std::isfinite(native[14])&&native[10]>0&&native[14]>0&&std::abs(native[11]+1)<1e-5,"NATIVE_REVERSE_Z_CLIP");
 const auto clipNear=native[14]/(1+native[10]),clipFar=native[14]/native[10];
 demand(clipNear>=.001&&clipNear<=1&&clipFar>clipNear&&clipFar<=100000,"NATIVE_CLIP_BOUND");
 const double l=std::tan(f.left),r=std::tan(f.right),u=std::tan(f.up),d=std::tan(f.down);Matrix p{};
 p[0]=2/(r-l);p[5]=2/(u-d);p[8]=(r+l)/(r-l);p[9]=(u+d)/(u-d);p[10]=clipNear/(clipFar-clipNear);p[11]=-1;p[14]=clipFar*clipNear/(clipFar-clipNear);return p;
}
struct FrameEye {Matrix view,vi,projection,delta;};
inline FrameEye eye(const Matrix&nativeView,const Matrix&nativeProjection,const Pose&recenter,const Pose&current,const Fov&fov){
 const auto local=mul(pose(current),inverse(pose(recenter)));
 const double distance=std::sqrt(local[12]*local[12]+local[13]*local[13]+local[14]*local[14]);
 const double cosAngle=(local[0]+local[5]+local[10]-1)*.5;
 // Diagnostic culling/comfort bound, not a claim that flat culling covers the whole interval.
 demand(distance<=.30&&cosAngle>=std::cos(30*3.141592653589793/180),"DEBUG_HMD_POSE_BOUND");
 FrameEye e;e.vi=mul(local,inverse(nativeView));e.view=inverse(e.vi);e.projection=projection(fov,nativeProjection);e.delta=mul(inverse(nativeView),e.view);return e;
}
inline MatrixBytes matrices(const MatrixBytes&raw,const Matrix&nativeProjection,const FrameEye&e,Kind kind){
 demand(kind>=Kind::Rigid&&kind<=Kind::VehicleCE6255,"UNSUPPORTED_CONSUMER");
 // Ped2381 does not consume the other complete matrices; preserve unused bytes,
 // including nonfinite sentinels, exactly as the proven typed contract requires.
 std::array<Matrix,4>m{};for(unsigned slot:{1u,2u}){
  if(slot==1&&kind!=Kind::Rigid)continue;
  for(unsigned i=0;i<16;++i){float f;memcpy(&f,raw.data()+64*slot+4*i,4);demand(std::isfinite(f),"NONFINITE_CONSUMED_MATRIX");m[slot][i]=f;}
 }MatrixBytes out=raw;
 const auto transform=mul(mul(inverse(nativeProjection),e.delta),e.projection);
 Matrix wvp=mul(m[2],transform),wv{};
 if(kind==Kind::Rigid)wv=mul(m[1],e.delta);
 auto store=[&](unsigned slot,const Matrix&v){for(unsigned i=0;i<16;++i){const float f=float(v[i]);demand(std::isfinite(f),"NONFINITE_XR_MATRIX");memcpy(out.data()+64*slot+4*i,&f,4);}};
 store(2,wvp);
 if(kind==Kind::Rigid)store(1,wv);
 if(kind!=Kind::Ped2381)store(3,e.vi);
 return out;
}
}
