#pragma once
#include "host_abi.h"
#include "source_host_properties.h"
#include "reset.h"
#include "menu_transport.h"
#include "weather_catalogue.h"
#include <math.h>
#include <float.h>
#include <string.h>
namespace menyoo_vr {
// Menyoo Weather controls/data, adapted to typed source properties. Exact
// captured override values (including 'no override') are reset, not current
// weather interpolation output. Rendering never calls these operations.
class WeatherActions {
 struct Property {WeatherActions*actions;unsigned index;};
 Host host_;ResetLedger*ledger_;uint64_t session_;bool permitted_,snowEnabled_;
 Property properties_[WeatherPropertyCount];
 int weatherIndices_[WeatherCatalogueCount],cloudIndices_[CloudCatalogueCount];
 static ResetAccess read(void*context,const OverrideOwner&owner,uint64_t&value){
  Property&p=*static_cast<Property*>(context);if(owner.session!=p.actions->session_)return ResetOwnerGone;
  return p.actions->query(SourceHostReadWeather+p.index,NULL,0,value)?ResetOk:ResetUnavailable;
 }
 static ResetAccess write(void*context,const OverrideOwner&owner,uint64_t value){
  Property&p=*static_cast<Property*>(context);if(owner.session!=p.actions->session_)return ResetOwnerGone;
  uint64_t output=0;return p.actions->query(SourceHostWriteWeather+p.index,&value,1,output)?ResetOk:ResetUnavailable;
 }
 bool change(unsigned property,uint64_t value,OverrideScope scope=ExplicitCheatOverride){
  if(!ledger_||property>=WeatherPropertyCount)return false;
  const OverrideOwner owner={session_,2,session_};Property&p=properties_[property];
  return ledger_->apply(100+property,owner,scope,&p,read,write,value)==ResetOk;
 }
 static uint64_t bits(float f){uint64_t v=0;memcpy(&v,&f,sizeof(f));return v;}
 static uint64_t bits(int n){uint64_t v=0;memcpy(&v,&n,sizeof(n));return v;}
 bool value(unsigned property,float&v){uint64_t raw=0;if(!query(SourceHostReadWeather+property,NULL,0,raw))return false;memcpy(&v,&raw,sizeof(v));return _finite(v)!=0;}
 bool value(unsigned property,int&v){uint64_t raw=0;if(!query(SourceHostReadWeather+property,NULL,0,raw))return false;memcpy(&v,&raw,sizeof(v));return true;}
 int resolve(uint64_t operation,const char*name){uint64_t raw=0,arg=reinterpret_cast<uintptr_t>(name);
  if(!query(operation,&arg,1,raw))return -2;int index=-1;memcpy(&index,&raw,sizeof(index));return index;}
 static void unavailable(Transport&m){m.rows.back().enabled=false;m.description("Unavailable in this build.");}
 bool number(Transport&m,const char*label,unsigned property,bool known,float displayed,float step,float lo,float hi,float multiplier=1){
  char text[64];sprintf_s(text,"%.2f",displayed);const bool chosen=m.add(label,NumberRow,text);
  if(!chosen||!known||(m.event()!=Left&&m.event()!=Right))return true;
  const float next=(m.event()==Right?displayed+step:displayed-step);
  const float clamped=next<lo?lo:(next>hi?hi:next);return change(property,bits(clamped/multiplier));
 }
public:
 WeatherActions():ledger_(NULL),session_(0),permitted_(false),snowEnabled_(false){memset(&host_,0,sizeof(host_));
  for(unsigned i=0;i<WeatherPropertyCount;++i){properties_[i].actions=this;properties_[i].index=i;}}
 bool bind(const Host&host,ResetLedger&ledger,uint64_t session){
  if(!host.invoke||!session)return false;host_=host;ledger_=&ledger;session_=session;permitted_=false;snowEnabled_=false;
  for(unsigned i=0;i<WeatherCatalogueCount;++i)weatherIndices_[i]=-2;
  for(unsigned i=0;i<CloudCatalogueCount;++i)cloudIndices_[i]=-2;
  return ledger.bindToggle(snowEnabled_,NULL);
 }
 void permit(bool enabled){permitted_=enabled;}
 bool query(uint64_t operation,const uint64_t*args,unsigned count,uint64_t&value){
  if(!permitted_||!host_.invoke)return false;uint64_t result[3]={};
  if(host_.invoke(host_.user,operation,args,count,result)!=Success)return false;value=result[0]&UINT64_C(0xffffffff);return true;
 }
 bool page(Transport&m,int cloudsPage){
  m.setTitle("Weather");bool ok=true;
  if(m.add("Reset Weather",ActionRow,"")&&m.event()==Accept)ok=change(WeatherTypeOverride,bits(-1))&&ok;
  m.icons(true,CrossIcon,CrossIcon);
  int current=-1;const bool currentKnown=value(WeatherCurrentType,current);
  for(unsigned i=0;i<WeatherCatalogueCount;++i){const WeatherName&w=WeatherCatalogue[i];
   if(weatherIndices_[i]==-2)weatherIndices_[i]=resolve(SourceHostResolveWeather,w.internal);const int index=weatherIndices_[i];
   if(m.add(w.label,ActionRow,"")&&m.event()==Accept&&index>=0)ok=change(WeatherTypeOverride,bits(index))&&ok;
   m.icons(currentKnown&&current==index,TickIcon,CrossIcon);if(index<0)unavailable(m);
  }
  int snow=0;const bool snowKnown=value(WeatherTerrainSnow,snow);
  if(m.add("Snow On Terrain",ToggleRow,snow?"ON":"OFF")&&m.event()==Accept&&snowKnown){
   ok=change(WeatherTerrainSnow,bits(snow?0:1),ToggleEffect)&&ok;if(ok)snowEnabled_=snow==0;
  }if(!snowKnown)unavailable(m);
  float wind=0,maximum=0,speed=0;const bool windKnown=value(WeatherWindOverride,wind)&&value(WeatherWindMaximum,maximum)&&maximum>0&&value(WeatherWindCurrent,speed);
  // Display native speed when no override exists; capture/reset the actual
  // override sentinel, never mistake the evolving weather speed for that state.
  ok=number(m,"Wind Speed",WeatherWindOverride,windKnown,wind<0?speed:wind*maximum,.1f,0,maximum,maximum>0?maximum:1)&&ok;
  if(!windKnown)unavailable(m);
  float ocean=0;const bool oceanKnown=value(WeatherOceanStrength,ocean);
  ok=number(m,"Ocean Wave Strength",WeatherOceanStrength,oceanKnown,ocean,.1f,0,1)&&ok;if(!oceanKnown)unavailable(m);
  float rain=0;const bool rainKnown=value(WeatherRainOverride,rain);
  ok=number(m,"Rain Puddles Multiplier",WeatherRainOverride,rainKnown,rain<0?0:rain,.1f,0,45)&&ok;if(!rainKnown)unavailable(m);
  float gravity=0;const bool gravityKnown=value(WeatherGravity,gravity);unsigned nearest=0;
  for(unsigned i=1;i<GravityCatalogueCount;++i)if(fabs(GravityCatalogue[i].value-gravity)<fabs(GravityCatalogue[nearest].value-gravity))nearest=i;
  if(m.add("Gravity Level",ChoiceRow,GravityCatalogue[nearest].label)&&(m.event()==Left||m.event()==Right)&&gravityKnown){
   if(m.event()==Right&&nearest+1<GravityCatalogueCount)++nearest;else if(m.event()==Left&&nearest)--nearest;
   ok=change(WeatherGravity,bits(GravityCatalogue[nearest].value))&&ok;
  }if(!gravityKnown)unavailable(m);
  m.add("Clouds",ActionRow,"",cloudsPage);return ok;
 }
 bool clouds(Transport&m){
  m.setTitle("Clouds");bool ok=true;
  if(m.add("Reset",ActionRow,"")&&m.event()==Accept)ok=change(WeatherScriptCloud,bits(-1))&&ok;m.icons(true,CrossIcon,CrossIcon);
  for(unsigned i=0;i<CloudCatalogueCount;++i){
   if(cloudIndices_[i]==-2)cloudIndices_[i]=resolve(SourceHostResolveCloud,CloudCatalogue[i]);const int index=cloudIndices_[i];
   if(m.add(CloudCatalogue[i],ActionRow,"")&&m.event()==Accept&&index>=0)ok=change(WeatherScriptCloud,bits(index))&&ok;
   if(index<0)unavailable(m);
  }return ok;
 }
};
}
