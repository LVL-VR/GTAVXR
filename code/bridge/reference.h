#pragma once
 
// Pure math only. Does not write native cameras or claim source visibility coverage.
#include "pose.h"
namespace reference084 {
using eye036::Matrix;
using Vec = std::array<double,3>;
inline double dot(const Vec&a,const Vec&b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline Vec cross(const Vec&a,const Vec&b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
inline Vec unit(Vec a){double n=std::sqrt(dot(a,a));pose075::demand(std::isfinite(n)&&n>1e-6,"REFERENCE_HEADING_DEGENERATE");for(auto&v:a)v/=n;return a;}
inline void rigid(const Matrix&m){
 for(double v:m)pose075::demand(std::isfinite(v),"REFERENCE_NONFINITE");
 pose075::demand(std::abs(m[3])+std::abs(m[7])+std::abs(m[11])+std::abs(m[15]-1)<1e-7,"REFERENCE_NOT_AFFINE");
 Vec rows[3];for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)rows[i][j]=m[4*i+j];
 for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)pose075::demand(std::abs(dot(rows[i],rows[j])-(i==j?1.:0.))<1e-4,"REFERENCE_NOT_ORTHONORMAL");
 pose075::demand(dot(cross(rows[0],rows[1]),rows[2])>0.9999,"REFERENCE_HANDEDNESS");
}
inline Matrix level(const Matrix&camera,const Vec&up){
 rigid(camera);pose075::demand(std::abs(dot(up,up)-1)<1e-12,"REFERENCE_UP_NOT_UNIT");
 Vec f{-camera[8],-camera[9],-camera[10]};double vertical=dot(f,up);
 for(unsigned i=0;i<3;++i)f[i]-=vertical*up[i];f=unit(f);Vec r=unit(cross(f,up));
 Matrix out=eye036::identity();for(unsigned i=0;i<3;++i){out[i]=r[i];out[4+i]=up[i];out[8+i]=-f[i];out[12+i]=camera[12+i];}return out;
}
inline Matrix nativeViewReference(const Matrix&nativeView){return level(eye036::inverse(nativeView),{0,0,1});}
inline Matrix xr_reference(const pose075::Pose&recenter){return level(pose075::pose(recenter),{0,1,0});}
inline pose075::FrameEye eye(const Matrix&nativeView,const Matrix&nativeProjection,
 const pose075::Pose&recenter,const pose075::Pose&current,const pose075::Fov&fov){
 const auto local=eye036::mul(pose075::pose(current),eye036::inverse(xr_reference(recenter)));
 pose075::FrameEye e;e.vi=eye036::mul(local,nativeViewReference(nativeView));e.view=eye036::inverse(e.vi);
 e.projection=pose075::projection(fov,nativeProjection);e.delta=eye036::mul(eye036::inverse(nativeView),e.view);return e;
}
}
