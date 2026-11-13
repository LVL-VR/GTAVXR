#pragma once
#include "host_abi.h"
#include "vehicle_host_abi.h"
#include <string.h>
namespace menyoo_vr {
class RetainedActions {
 Host host_;bool permitted_;
public:
 RetainedActions():permitted_(false){memset(&host_,0,sizeof(host_));}
 void bind(const Host&host){host_=host;permitted_=false;}
 void permit(bool enabled){permitted_=enabled;}
 int apply(unsigned action){if(!permitted_||!host_.invoke)return WrongContext;if(action>=TrainerActionCount)return InvalidInput;
  const uint64_t arg=action;uint64_t r[3]={};return host_.invoke(host_.user,SourceHostTrainerAction,&arg,1,r);}
 bool wanted(unsigned&level){if(!permitted_||!host_.invoke)return false;uint64_t r[3]={};if(host_.invoke(host_.user,SourceHostWantedRead,NULL,0,r)!=Success||r[0]>5)return false;level=static_cast<unsigned>(r[0]);return true;}
 bool adjustWanted(bool up){if(!permitted_||!host_.invoke)return false;const uint64_t arg=up?1:0;uint64_t r[3]={};return host_.invoke(host_.user,SourceHostWantedAdjust,&arg,1,r)==Success;}
 bool setWanted(unsigned level){if(!permitted_||!host_.invoke||level>5)return false;const uint64_t arg=level;uint64_t r[3]={};return host_.invoke(host_.user,SourceHostWantedSet,&arg,1,r)==Success;}
};
}
