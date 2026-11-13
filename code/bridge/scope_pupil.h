#pragma once
#include "scope_eye_math.h"
namespace gx_scope_pupil {
using gx_scope_eye::V;
struct Lens {
 V center;gx_scope_eye::Axes axes;double radius,minEyeRelief,maxEyeRelief;bool geometryQualified;
 Lens():radius(0),minEyeRelief(0),maxEyeRelief(0),geometryQualified(false){}
 bool valid()const{return geometryQualified&&gx_scope_eye::finite(center)&&axes.valid()&&gx_scope_eye::finite(radius)&&radius>0&&gx_scope_eye::finite(minEyeRelief)&&gx_scope_eye::finite(maxEyeRelief)&&minEyeRelief>0&&maxEyeRelief>=minEyeRelief;}
};
struct Entry {bool visible;V point;double relief,u,v;Entry():visible(false),relief(0),u(0),v(0){}};
// Eye/head forward supplies gaze approximation; no eye tracker or pose writer.
// Geometry/relief must come from qualified model/profile data, not camera framing offsets.
inline Entry enter(const Lens&lens,V eye,V gaze){
 Entry out;if(!lens.valid()||!gx_scope_eye::finite(eye)||!gx_scope_eye::finite(gaze)||fabs(gx_scope_eye::dot(gaze,gaze)-1)>1e-5)return out;
 V toPlane=gx_scope_eye::add(lens.center,gx_scope_eye::scale(eye,-1));
 double relief=gx_scope_eye::dot(toPlane,lens.axes.forward),den=gx_scope_eye::dot(gaze,lens.axes.forward);
 if(relief<lens.minEyeRelief||relief>lens.maxEyeRelief||den<=1e-8)return out;
 out.point=gx_scope_eye::add(eye,gx_scope_eye::scale(gaze,relief/den));
 V delta=gx_scope_eye::add(out.point,gx_scope_eye::scale(lens.center,-1));
 double x=gx_scope_eye::dot(delta,lens.axes.right),y=gx_scope_eye::dot(delta,lens.axes.up);
 if(!gx_scope_eye::finite(out.point)||x*x+y*y>lens.radius*lens.radius)return Entry();
 out.relief=relief;out.u=x/lens.radius;out.v=y/lens.radius;out.visible=true;return out;
}
}
