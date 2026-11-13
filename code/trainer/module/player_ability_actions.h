#pragma once
#include "host_abi.h"
#include "reset.h"
#include "menu_transport.h"
#include "player_ability_abi.h"
#include <string.h>
namespace menyoo_vr {
// Retained Menyoo Player abilities (Trainer V3 block P04): Super Run, Walk underwater, Forcefield, Smash Ability,
// Fly Auto and Ignored By Everyone. The module owns the toggles; the native host runs the source behaviour once per
// UPDATE and restores its own captures when a bit turns off. Reset Toggles and module close turn every one off.
class PlayerAbilityActions {
 Host host_;bool permitted_,superRun_,walk_,forcefieldOn_,smash_,fly_,ignored_;unsigned forcefield_;
 uint64_t lastGeneration_;uint32_t applied_;
 bool call(uint32_t mask,unsigned mode){
  if(!permitted_||!host_.invoke)return false;uint64_t args[2]={mask,mode},result[3]={};
  return host_.invoke(host_.user,SourceHostPlayerAbilities,args,2,result)==Success;
 }
 uint32_t mask()const{return (superRun_?AbilitySuperRun:0u)|(walk_?AbilityWalkUnderwater:0u)|(forcefield_?AbilityForcefield:0u)|
  (smash_?AbilitySmash:0u)|(fly_?AbilityFlyAuto:0u)|(ignored_?AbilityIgnored:0u);}
 static void toggle(Transport&m,const char*label,bool&value,const char*help){
  if(m.add(label,ToggleRow,value?"ON":"OFF")&&m.event()==Accept)value=!value;m.description(help);
 }
public:
 PlayerAbilityActions():permitted_(false),superRun_(false),walk_(false),forcefieldOn_(false),smash_(false),fly_(false),ignored_(false),
  forcefield_(0),lastGeneration_(0),applied_(0){memset(&host_,0,sizeof(host_));}
 bool bind(const Host&host,ResetLedger&ledger){
  host_=host;permitted_=false;superRun_=walk_=forcefieldOn_=smash_=fly_=ignored_=false;forcefield_=0;lastGeneration_=0;applied_=0;
  return ledger.bindToggle(superRun_,NULL)&&ledger.bindToggle(walk_,NULL)&&ledger.bindToggle(forcefieldOn_,NULL)&&
   ledger.bindToggle(smash_,NULL)&&ledger.bindToggle(fly_,NULL)&&ledger.bindToggle(ignored_,NULL);
 }
 void permit(bool enabled){permitted_=enabled;}
 void superRun(Transport&m){toggle(m,"Super Run",superRun_,"Hold Sprint to run much faster.");}
 void walkUnderwater(Transport&m){toggle(m,"Walk underwater",walk_,"Walk on the bottom instead of swimming. Turning it off restores the native swim flags.");}
 void forcefield(Transport&m){
  static const char*const names[ForcefieldModeCount]={"Off","Push Out","Destroy"};
  if(!forcefieldOn_)forcefield_=0;
  if(m.add("Forcefield",ChoiceRow,names[forcefield_<ForcefieldModeCount?forcefield_:0])){
   if(m.event()==Right||m.event()==Accept)forcefield_=(forcefield_+1)%ForcefieldModeCount;
   else if(m.event()==Left)forcefield_=(forcefield_+ForcefieldModeCount-1)%ForcefieldModeCount;
   forcefieldOn_=forcefield_!=0;
  }
  m.description("Push Out shoves nearby people, vehicles and objects away. Destroy surrounds you with explosions while you stay explosion-proof.");
 }
 void smash(Transport&m){toggle(m,"Smash Ability",smash_,"Hold Jump to lift everything nearby, release Jump to slam it down. Jump is used by the ability while it is on.");}
 void flyAuto(Transport&m){toggle(m,"Fly Auto",fly_,"Launches you with a parachute and flies you forward. Hold A to brake.");}
 void ignored(Transport&m){toggle(m,"Ignored By Everyone",ignored_,"Police and other people ignore you; nearby people who are not part of a mission stay calm.");}
 bool tick(uint64_t generation){
  if(!permitted_||!generation)return false;if(lastGeneration_==generation)return true;lastGeneration_=generation;
  if(!forcefieldOn_)forcefield_=0;const uint32_t wanted=mask();if(!wanted&&!applied_)return true;
  if(!call(wanted,forcefield_))return false;applied_=wanted;return true;
 }
 bool release(){
  superRun_=walk_=forcefieldOn_=smash_=fly_=ignored_=false;forcefield_=0;if(!applied_)return true;
  if(!call(0,0))return false;applied_=0;return true;
 }
 void suspend(){superRun_=walk_=forcefieldOn_=smash_=fly_=ignored_=false;forcefield_=0;}
};
}
