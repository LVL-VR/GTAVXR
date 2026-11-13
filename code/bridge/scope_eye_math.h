#pragma once
// Independently authored angular optic sampler. No camera/pose/state writes.
// SOURCE-EYE CROP PROTOTYPE ONLY: translational parallax/high-zoom pixels are not synthesized.
#include <math.h>
#include <float.h>
#include <stdint.h>
namespace gx_scope_eye {
struct V {double x,y,z;V():x(0),y(0),z(0){}V(double a,double b,double c):x(a),y(b),z(c){}};
inline double dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline V add(V a,V b){return V(a.x+b.x,a.y+b.y,a.z+b.z);}
inline V scale(V a,double s){return V(a.x*s,a.y*s,a.z*s);}
inline bool finite(double x){return _finite(x)!=0;}
inline bool finite(V v){return finite(v.x)&&finite(v.y)&&finite(v.z);}
struct Axes {V right,up,forward;bool valid()const{return finite(right)&&finite(up)&&finite(forward)&&fabs(dot(right,right)-1)<1e-5&&fabs(dot(up,up)-1)<1e-5&&fabs(dot(forward,forward)-1)<1e-5&&fabs(dot(right,up))<1e-5&&fabs(dot(right,forward))<1e-5&&fabs(dot(up,forward))<1e-5;}};
struct Fov {double left,right,up,down;Fov():left(0),right(0),up(0),down(0){}bool valid()const{return finite(left)&&finite(right)&&finite(up)&&finite(down)&&left> -1.5707963267948966&&right<1.5707963267948966&&down> -1.5707963267948966&&up<1.5707963267948966&&left<right&&down<up;}};
struct UV {bool valid;double u,v;UV():valid(false),u(0),v(0){}};
struct Mapping {V rowU,rowV,rowQ;bool valid;Mapping():valid(false){}};
// Output x/y in[-1,+1] on lens. Coefficients sample this exact source eye.
inline UV sample(const Mapping&m,double x,double y){UV o;if(!m.valid||!finite(x)||!finite(y)||x*x+y*y>1.0000001)return o;V p(x,y,1);double q=dot(m.rowQ,p);if(!finite(q)||q<=1e-8)return o;o.u=dot(m.rowU,p)/q;o.v=dot(m.rowV,p)/q;o.valid=finite(o.u)&&finite(o.v)&&o.u>=0&&o.u<=1&&o.v>=0&&o.v<=1;return o;}
inline bool build(const Axes&eye,const Fov&fov,const Axes&optic,double baseHalfAngle,double magnification,Mapping&out){
 out=Mapping();if(!eye.valid()||!optic.valid()||!fov.valid()||!finite(baseHalfAngle)||baseHalfAngle<=0||baseHalfAngle>=1.5707963267948966||!finite(magnification)||magnification<1)return false;
 double l=tan(fov.left),r=tan(fov.right),u=tan(fov.up),d=tan(fov.down),t=tan(baseHalfAngle)/magnification;
 if(!finite(t)||!finite(l)||!finite(r)||!finite(u)||!finite(d)||r<=l||u<=d)return false;
 V rays[3]={scale(optic.right,t),scale(optic.up,t),optic.forward};double a[3],b[3],q[3];
 for(unsigned i=0;i<3;++i){q[i]=dot(eye.forward,rays[i]);a[i]=(dot(eye.right,rays[i])-l*q[i])/(r-l);b[i]=(u*q[i]-dot(eye.up,rays[i]))/(u-d);}
 out.rowU=V(a[0],a[1],a[2]);out.rowV=V(b[0],b[1],b[2]);out.rowQ=V(q[0],q[1],q[2]);out.valid=finite(out.rowU)&&finite(out.rowV)&&finite(out.rowQ);
 // Refuse incomplete crop before publication; NEVER clamp to unrelated edge pixels.
 const double coords[3]={-1,0,1};for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j){V p(coords[i],coords[j],1);double den=dot(out.rowQ,p);if(den<=1e-8||!finite(den)){out=Mapping();return false;}double su=dot(out.rowU,p)/den,sv=dot(out.rowV,p)/den;if(!finite(su)||!finite(sv)||su<0||su>1||sv<0||sv>1){out=Mapping();return false;}}
 return true;
}
// D3D11 b0: exactly three float4 rows, with explicit padding. Never memcpy double Mapping.
inline bool constants(const Mapping&m,float*out){
 if(!out)return false;for(unsigned i=0;i<12;++i)out[i]=0;
 if(!m.valid||!finite(m.rowU)||!finite(m.rowV)||!finite(m.rowQ))return false;
 const V rows[3]={m.rowU,m.rowV,m.rowQ};
 for(unsigned r=0;r<3;++r){out[r*4]=(float)rows[r].x;out[r*4+1]=(float)rows[r].y;out[r*4+2]=(float)rows[r].z;}
 for(unsigned i=0;i<12;++i)if(!_finite(out[i])){for(unsigned j=0;j<12;++j)out[j]=0;return false;}
 return true;
}
struct Image {unsigned eye,width,height;uint64_t frame,generation;bool owned;Image():eye(2),width(0),height(0),frame(0),generation(0),owned(false){}};
inline bool pairCurrent(const Image*a,uint64_t frame,uint64_t generation){return a&&frame&&generation&&a[0].owned&&a[1].owned&&a[0].eye==0&&a[1].eye==1&&a[0].width&&a[0].height&&a[1].width&&a[1].height&&a[0].frame==frame&&a[1].frame==frame&&a[0].generation==generation&&a[1].generation==generation;}
}
