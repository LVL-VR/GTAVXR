#pragma once
#include "pose.h"
namespace ready078 {
// Local transport evidence: first pre-FOCUSED pose was valid/tracked yet jumped >1m.
// Never widen motion bounds. Establish the reference only from three stable focused poses.
struct Gate {
 unsigned stable=0;
 pose075::Pose previous{};
 bool accept(bool focused,bool tracked,const std::array<pose075::Pose,2>&eyes){
  if(!focused||!tracked){stable=0;return false;}
  for(const auto&p:eyes)(void)pose075::pose(p); // finite/unit quaternion, before arithmetic
  auto center=eyes[0];const auto&r=eyes[1];
  const double dx=center.x-r.x,dy=center.y-r.y,dz=center.z-r.z;
  const double ipd=std::sqrt(dx*dx+dy*dy+dz*dz);
  if(ipd<.045||ipd>.085){stable=0;return false;}
  const double parallel=std::abs(center.qx*r.qx+center.qy*r.qy+center.qz*r.qz+center.qw*r.qw);
  if(parallel<.99999){stable=0;return false;}
  center.x=(center.x+r.x)*.5;center.y=(center.y+r.y)*.5;center.z=(center.z+r.z)*.5;
  const double x=center.x-previous.x,y=center.y-previous.y,z=center.z-previous.z;
  const double dot=std::min(1.,std::abs(center.qx*previous.qx+center.qy*previous.qy+center.qz*previous.qz+center.qw*previous.qw));
  const bool close=x*x+y*y+z*z<=.02*.02&&dot>=std::cos(1.*3.141592653589793/180.); // quaternion half-angle: 2deg
  stable=stable&&close?stable+1:1;previous=center;return stable>=3;
 }
};
}
