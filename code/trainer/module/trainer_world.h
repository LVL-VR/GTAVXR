#pragma once
// shared helpers for the retained Vehicle/Weapon/Teleport pages. Every game operation is a
// native through the generated typed binding (source_natives.h, exact source registrations) or a typed host operation
// (trainer_world_abi.h). Menyoo's retail-memory helpers (GTAmemory pools, handling pointers) are never used.
#include "source_natives.h"
#include "trainer_world_abi.h"
#include <math.h>
#include <string.h>
#include <string>
#include <vector>
namespace menyoo_vr { namespace tw {
using sn::V3;
enum Control { INPUT_SPRINT=21,INPUT_JUMP=22,INPUT_ATTACK=24,INPUT_AIM=25,INPUT_LOOK_BEHIND=26,INPUT_CONTEXT=51,
 INPUT_VEH_ATTACK=69,INPUT_VEH_ACCELERATE=71,INPUT_VEH_BRAKE=72,INPUT_VEH_EXIT=75,INPUT_VEH_HANDBRAKE=76,
 INPUT_VEH_LOOK_BEHIND=79,INPUT_VEH_HORN=86,INPUT_PARACHUTE_DEPLOY=144,INPUT_FRONTEND_RDOWN=191,INPUT_FRONTEND_LS=209,
 INPUT_FRONTEND_RS=210,INPUT_SCRIPT_LEFT_AXIS_X=218,INPUT_SCRIPT_LEFT_AXIS_Y=219 };
inline V3 v3(float x,float y,float z){V3 v={x,y,z};return v;}
inline V3 add(const V3&a,const V3&b){return v3(a.x+b.x,a.y+b.y,a.z+b.z);}
inline V3 sub(const V3&a,const V3&b){return v3(a.x-b.x,a.y-b.y,a.z-b.z);}
inline V3 mul(const V3&a,float s){return v3(a.x*s,a.y*s,a.z*s);}
inline float length(const V3&a){return sqrtf(a.x*a.x+a.y*a.y+a.z*a.z);}
inline float distance(const V3&a,const V3&b){return length(sub(a,b));}
inline V3 normal(const V3&a){const float l=length(a);return l>1e-6f?mul(a,1.f/l):v3(0,0,0);}
// Menyoo Vector3::RotationToDirection (degrees, rotation order 2).
inline V3 direction(const V3&rotation){const float z=rotation.z*.0174532924f,x=rotation.x*.0174532924f,n=fabsf(cosf(x));
 return v3(-sinf(z)*n,cosf(z)*n,sinf(x));}
// Notification sink: the module routes these into the existing localized action message.
typedef void(*Notifier)(const char*);
inline Notifier&notifier(){static Notifier n=0;return n;}
inline void notify(const char*text){if(notifier())notifier()(text);}
inline int ped(){return sn::PLAYER_PED_ID();}
inline int player(){return sn::PLAYER_ID();}
inline bool exists(int entity){return entity>0&&sn::DOES_ENTITY_EXIST(entity);}
inline int vehicle(){const int p=ped();return p>0&&sn::IS_PED_IN_ANY_VEHICLE(p,false)?sn::GET_VEHICLE_PED_IS_IN(p,false):0;}
inline unsigned model(int entity){return static_cast<unsigned>(sn::GET_ENTITY_MODEL(entity));}
inline bool heli(unsigned m){return sn::IS_THIS_MODEL_A_HELI(static_cast<int>(m));}
inline bool plane(unsigned m){return sn::IS_THIS_MODEL_A_PLANE(static_cast<int>(m));}
inline bool boat(unsigned m){return sn::IS_THIS_MODEL_A_BOAT(static_cast<int>(m))||sn::IS_THIS_MODEL_A_JETSKI(static_cast<int>(m));}
inline bool bike(unsigned m){return sn::IS_THIS_MODEL_A_BIKE(static_cast<int>(m))||sn::IS_THIS_MODEL_A_BICYCLE(static_cast<int>(m));}
inline bool cargobob(unsigned m){return m==0xfcfcb68bu||m==0x60a7ea10u||m==0x53174eefu||m==0x78bc1a3cu;}
inline void dims(unsigned m,V3&low,V3&high){low=v3(0,0,0);high=v3(0,0,0);sn::GET_MODEL_DIMENSIONS(static_cast<int>(m),&low,&high);}
// Menyoo ModelDimensions: Dim1 = max corner, Dim2 = -min corner (both positive extents).
inline void extents(unsigned m,V3&front,V3&back){V3 low,high;dims(m,low,high);front=high;back=mul(low,-1.f);}
inline bool bone(int entity,const char*name){return sn::GET_ENTITY_BONE_INDEX_BY_NAME(entity,name)!=-1;}
inline V3 position(int entity){return sn::GET_ENTITY_COORDS(entity,true);}
inline V3 offset(int entity,float x,float y,float z){return sn::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(entity,v3(x,y,z));}
inline int timer(){return sn::GET_GAME_TIMER();}
// APPLY_FORCE_TO_ENTITY(entity,type,force,offset,component,isLocal,isDirRel,isForce,ignoreUpVec,isRateIndependent).
inline void force(int entity,int type,const V3&f,const V3&o,int component,bool local,bool relative,bool isForce,bool up,bool rate){
 sn::APPLY_FORCE_TO_ENTITY(entity,type,f,o,component,local,relative,isForce,up,rate);}
// GTAentity::ApplyForce(direction) = type 3, world direction, isForce=false semantics of Menyoo.
inline void push(int entity,const V3&along){force(entity,3,along,v3(0,0,0),0,false,true,true,false,true);}
// GTAentity::ApplyForceRelative(direction) = type 3, local.
inline void pushRelative(int entity,const V3&along){force(entity,3,along,v3(0,0,0),0,true,true,true,false,true);}
inline bool requestModel(unsigned m){if(!sn::IS_MODEL_VALID(static_cast<int>(m)))return false;if(sn::HAS_MODEL_LOADED(static_cast<int>(m)))return true;sn::REQUEST_MODEL(static_cast<int>(m));return false;}
inline std::string text(const char*label,const char*fallback){
 if(!label||!*label)return fallback?fallback:"";
 if(!sn::DOES_TEXT_LABEL_EXIST(label))return fallback?fallback:label;
 const char*value=sn::GET_FILENAME_FOR_AUDIO_CONVERSATION(label);return value&&*value?std::string(value):std::string(fallback?fallback:label);
}
// Typed host query (trainer_world_abi.h): source pool entities within a radius of a point, excluding the player.
inline unsigned nearby(unsigned types,const V3&centre,float radius,std::vector<int>&out,unsigned capacity=96){
 out.assign(capacity,0);EntityQuery q={};q.size=sizeof(q);q.types=types;q.x=centre.x;q.y=centre.y;q.z=centre.z;q.radius=radius;
 q.capacity=capacity;q.handles=out.data();uint64_t arg=static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&q)),r[3]={};
 if(!sn::call(SourceHostEntityQuery,&arg,1,r)||q.count>capacity){out.clear();return 0;}
 out.resize(q.count);return q.count;
}
inline int closest(unsigned types,const V3&centre,float radius,int except=0){
 std::vector<int>h;nearby(types,centre,radius,h);int best=0;float bestDistance=radius*radius+1;
 for(size_t i=0;i<h.size();++i){if(h[i]==except||!exists(h[i]))continue;const V3 d=sub(position(h[i]),centre);const float d2=d.x*d.x+d.y*d.y+d.z*d.z;
  if(d2<bestDistance){bestDistance=d2;best=h[i];}}
 return best;
}
// Menyoo TickRainbowFader: one RGB step every 20 ms.
struct Fader {int r,g,b,last;Fader():r(255),g(0),b(0),last(0){}
 void tick(int now){if(now-last<20&&last)return;last=now;
  if(r>0&&b==0){--r;++g;}if(g>0&&r==0){--g;++b;}if(b>0&&g==0){++r;--b;}}
};
inline Fader&fader(){static Fader f;return f;}
inline int firstFreeSeat(int vehicle,int fallback){const int maxPass=sn::GET_VEHICLE_MAX_NUMBER_OF_PASSENGERS(vehicle);
 for(int i=-1;i<=maxPass;++i)if(sn::IS_VEHICLE_SEAT_FREE(vehicle,i,false))return i;return fallback;}
inline void setInvincible(int v,bool on){
 if(on){sn::SET_VEHICLE_CAN_BREAK(v,false);sn::SET_VEHICLE_CAN_BE_VISIBLY_DAMAGED(v,false);sn::SET_VEHICLE_TYRES_CAN_BURST(v,false);
  sn::SET_ENTITY_CAN_BE_DAMAGED(v,false);sn::SET_ENTITY_INVINCIBLE(v,true);sn::SET_ENTITY_PROOFS(v,true,false,true,true,true,true,true,true);}
 else{sn::SET_VEHICLE_CAN_BREAK(v,true);sn::SET_VEHICLE_CAN_BE_VISIBLY_DAMAGED(v,true);sn::SET_ENTITY_CAN_BE_DAMAGED(v,true);
  sn::SET_ENTITY_INVINCIBLE(v,false);sn::SET_ENTITY_PROOFS(v,false,false,false,false,false,false,false,false);sn::SET_ENTITY_ONLY_DAMAGED_BY_PLAYER(v,false);}
}
inline void repair(int v,float health){sn::SET_VEHICLE_FIXED(v);sn::SET_VEHICLE_DIRT_LEVEL(v,0.f);sn::SET_VEHICLE_ENGINE_HEALTH(v,health);
 sn::SET_VEHICLE_PETROL_TANK_HEALTH(v,health);sn::SET_VEHICLE_BODY_HEALTH(v,health);sn::SET_VEHICLE_UNDRIVEABLE(v,false);}
}}
