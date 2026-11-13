#pragma once
#include "locomotion.h"
namespace gx_pause_anchor {
class Anchor {
    GxPose pose_;bool valid_;
public:
    Anchor():valid_(false){pose_=GxPose();}
    void reset(){valid_=false;}
    bool valid()const{return valid_;}
    const GxPose&pose()const{return pose_;}
    bool enter(const GxPose&head){
        if(valid_)return true;
        const double yaw=gx_loco::yaw(head);
        if(!_finite(head.x)||!_finite(head.y)||!_finite(head.z)||!_finite(yaw))return false;
        const double norm=head.qx*head.qx+head.qy*head.qy+head.qz*head.qz+head.qw*head.qw;
        if(!_finite(norm)||norm<.5||norm>1.5)return false;
        pose_=head;pose_.qx=pose_.qz=0;pose_.qy=sin(yaw*.5);pose_.qw=cos(yaw*.5);
        pose_.x-=2*sin(yaw);pose_.z-=2*cos(yaw);valid_=true;return true;
    }
};
}
