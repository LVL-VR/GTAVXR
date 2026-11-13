#pragma once
// Native-only copy of a decision ALREADY used by gameplay. This is not a
// target finder or a second interaction resolver. Labels are copied, not pointers.
#include <stdint.h>
#include <string.h>
namespace gx_hand_context {
enum Owner {FREE,WEAPON,SUPPORT,PHONE,VEHICLE,PHYSICS,MISSION,MENU,CONFLICT};
enum Action {NONE,PHONE_NAVIGATE,RELEASE_SUPPORT,RELEASE_CONTROL,SPECIAL_ABILITY,GRAB,RELEASE,TWO_HAND_GRAB};
enum Input {NO_INPUT,RIGHT_A_B_STICK,LEFT_TRIGGER_RELEASE,HAND_GRIP_RELEASE,BOTH_CLICKS_RELEASE,RIGHT_TRIGGER_RELEASE,HAND_GRIP_PRESS,DOUBLE_LCLICK};
struct Candidate {
    uint64_t targetId,sequence,generation;
    unsigned identityEpoch,targetRevision,hand,priority;
    Owner owner;Action action;Input input;
    bool canExecute,availabilityKnown,pressed;
    char label[64],binding[96];
    Candidate(){clear();}
    void clear(){memset(this,0,sizeof(*this));}
};
inline bool knownAction(Owner owner,Action action,Input input,unsigned hand){
    if(hand>=2)return false;
    return (owner==PHONE&&hand==1&&action==PHONE_NAVIGATE&&input==RIGHT_A_B_STICK)||
        (owner==SUPPORT&&hand==0&&action==RELEASE_SUPPORT&&input==LEFT_TRIGGER_RELEASE)||
        (owner==SUPPORT&&hand==1&&action==RELEASE_SUPPORT&&input==RIGHT_TRIGGER_RELEASE)||
        (owner==VEHICLE&&action==RELEASE_CONTROL&&input==HAND_GRIP_RELEASE)||
        (owner==FREE&&hand==0&&action==SPECIAL_ABILITY&&input==DOUBLE_LCLICK)||
        (owner==PHYSICS&&action==RELEASE&&input==HAND_GRIP_RELEASE)||
        (owner==PHYSICS&&(action==GRAB||action==TWO_HAND_GRAB)&&input==HAND_GRIP_PRESS);
}
inline bool ready(const Candidate&c,unsigned hand,Owner owner,uint64_t sequence,
                  unsigned identityEpoch,uint64_t generation,unsigned validHands){
    return hand<2&&(validHands&(1u<<hand))&&c.hand==hand&&c.owner==owner&&owner<CONFLICT&&
        knownAction(c.owner,c.action,c.input,c.hand)&&c.canExecute&&c.availabilityKnown&&
        c.targetId&&c.targetRevision&&c.sequence&&c.sequence==sequence&&
        identityEpoch&&c.identityEpoch==identityEpoch&&generation&&c.generation==generation&&
        c.priority<=1000&&c.label[0]&&c.binding[0]&&
        memchr(c.label,0,sizeof(c.label))&&memchr(c.binding,0,sizeof(c.binding));
}
inline void copyLabel(char*out,unsigned capacity,const char*text){
    if(!capacity)return;unsigned i=0;
    // Contract labels contain only printable ASCII and are fixed by their
    // actual producer. No world/user text or Scaleform control escapes enter.
    for(;text&&text[i]&&i+1<capacity;++i){const unsigned c=(unsigned char)text[i];out[i]=(char)(c>=32&&c<=126?c:'?');}
    out[i]=0;
}
class Publication {
    Candidate candidates_[2];Owner owners_[2];
    uint64_t sequence_,generation_,tokens_[2];unsigned epoch_,mask_,revisions_[2];
    Owner tokenOwners_[2];
public:
    Publication(){memset(tokens_,0,sizeof(tokens_));memset(revisions_,0,sizeof(revisions_));
        tokenOwners_[0]=tokenOwners_[1]=FREE;clear();}
    void clear(){sequence_=generation_=epoch_=mask_=0;
        for(unsigned h=0;h<2;++h){candidates_[h].clear();owners_[h]=MISSION;
            tokens_[h]=0;tokenOwners_[h]=FREE;if(++revisions_[h]==0)++revisions_[h];}}
    void begin(uint64_t sequence,unsigned epoch,uint64_t generation,unsigned hands,Owner left,Owner right){
        if(!sequence||!epoch||!generation||(hands&~3u)){clear();return;}
        if(epoch_!=epoch||sequence_>sequence)clear();
        sequence_=sequence;epoch_=epoch;generation_=generation;mask_=hands;
        owners_[0]=left;owners_[1]=right;for(unsigned h=0;h<2;++h)candidates_[h].clear();
    }
    Owner owner(unsigned hand)const{return hand<2?owners_[hand]:CONFLICT;}
    // Exact native producers claim only their ACTUAL resolved ownership. A
    // disagreement suppresses the display; this store never awards gameplay
    // ownership or guesses a priority winner different from the real resolver.
    bool claim(unsigned hand,Owner owner){
        if(hand>=2||!(mask_&(1u<<hand))||!sequence_||!epoch_||!generation_||owner>=CONFLICT)return false;
        if(owners_[hand]!=FREE&&owners_[hand]!=owner){owners_[hand]=CONFLICT;candidates_[hand].clear();return false;}
        owners_[hand]=owner;return true;
    }
    bool publish(unsigned hand,Owner owner,Action action,Input input,uint64_t nativeToken,
                 unsigned priority,bool canExecute,bool known,bool pressed,const char*label,const char*binding){
        if(!knownAction(owner,action,input,hand)||!nativeToken||!canExecute||!known||priority>1000)return false;
        if(!claim(hand,owner))return false;
        if(tokens_[hand]!=nativeToken||tokenOwners_[hand]!=owner){tokens_[hand]=nativeToken;tokenOwners_[hand]=owner;
            if(++revisions_[hand]==0)++revisions_[hand];}
        Candidate&c=candidates_[hand];c.clear();c.targetId=nativeToken;c.targetRevision=revisions_[hand];
        c.hand=hand;c.owner=owner;c.action=action;c.input=input;c.priority=priority;c.sequence=sequence_;
        c.identityEpoch=epoch_;c.generation=generation_;c.canExecute=canExecute;c.availabilityKnown=known;c.pressed=pressed;
        copyLabel(c.label,sizeof(c.label),label);copyLabel(c.binding,sizeof(c.binding),binding);return c.label[0]&&c.binding[0];
    }
    bool read(unsigned hand,uint64_t sequence,unsigned epoch,uint64_t generation,unsigned validHands,Candidate&out)const{
        out.clear();if(hand>=2||sequence_!=sequence||epoch_!=epoch||generation_!=generation)return false;
        if(!ready(candidates_[hand],hand,owners_[hand],sequence,epoch,generation,mask_&validHands))return false;
        out=candidates_[hand];return true;
    }
};
}
