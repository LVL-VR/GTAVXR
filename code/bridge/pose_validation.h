#pragma once
#include <cmath>
#include <float.h>

namespace gx_pose {
enum Validation { Rejected, Accepted, Renormalized };
inline bool finite(double value) {
#if defined(_MSC_VER) && _MSC_VER < 1800
    return _finite(value)!=0;
#else
    return std::isfinite(value);
#endif
}
// All callers validate before pose075 math. Rejection never changes the pose.
template<class Pose> Validation validate(Pose& p) {
    if(!finite(p.x)||!finite(p.y)||!finite(p.z)||!finite(p.qx)||
       !finite(p.qy)||!finite(p.qz)||!finite(p.qw))return Rejected;
    const double n=p.qx*p.qx+p.qy*p.qy+p.qz*p.qz+p.qw*p.qw;
    if(!finite(n)||std::abs(n-1)>.01)return Rejected;
    if(n==1)return Accepted;
    const double k=1/std::sqrt(n);
    p.qx*=k;p.qy*=k;p.qz*=k;p.qw*=k;
    return Renormalized;
}
}
