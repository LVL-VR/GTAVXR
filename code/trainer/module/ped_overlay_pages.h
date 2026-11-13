#pragma once
#include "host_abi.h"
#include "menu_transport.h"
#include "source_ped_commands.h"
#include "source_replenish_commands.h"
#include "ped_decal_catalogue.h"
#include <map>
#include <vector>
#include <string.h>
namespace menyoo_vr {
// retained Wardrobe Decal Overlays and Damage Overlays (Trainer V3 block P03), from Menyoo
// PedComponentChanger.cpp PedDecals_catind / PedDamageTextures_catind with the pinned PedDecalOverlays.xml data.
// Applied decals and packs are tracked per ped handle exactly like the source (removal clears and re-adds the rest).
// These are one-shot appearance edits: Reset does not undo them (source behaviour).
class PedOverlayPages {
 struct Decal {uint32_t collection,name;};
 Host host_;bool permitted_;unsigned type_,zone_;
 std::map<int,std::vector<Decal> > decals_;std::map<int,std::vector<unsigned> > packs_;
 bool call(uint64_t hash,const uint64_t*args,unsigned count,int*value=NULL){
  if(!permitted_||!host_.invoke)return false;uint64_t result[3]={};
  if(host_.invoke(host_.user,hash,args,count,result)!=Success)return false;if(value)memcpy(value,result,sizeof(*value));return true;
 }
 bool ped(int&handle,uint32_t&model){
  int raw=0;if(!call(source_ped::PLAYER_PED_ID,NULL,0,&handle)||handle<=0)return false;
  const uint64_t arg=static_cast<uint32_t>(handle);if(!call(source_decal::GET_ENTITY_MODEL,&arg,1,&raw))return false;
  model=static_cast<uint32_t>(raw);return true;
 }
 static const char*typeName(unsigned t){return t>=PedDecalTypeCount?"":(strncmp(PedDecalTypes[t],"USERTYPE_",9)==0?"Hair":PedDecalTypes[t]);}
 // "Back" would collide with the navigation label translation; show the body zone explicitly.
 static const char*zoneName(unsigned z){return z>=PedDecalZoneCount?"":(strcmp(PedDecalZones[z],"Back")==0?"Upper Back":PedDecalZones[z]);}
 static bool hasType(uint32_t model,unsigned t){for(unsigned i=0;i<PedDecalCount;++i)if(PedDecalCatalogue[i].model==model&&PedDecalCatalogue[i].type==t)return true;return false;}
 static bool hasZone(uint32_t model,unsigned t,unsigned z){for(unsigned i=0;i<PedDecalCount;++i){const PedDecalEntry&e=PedDecalCatalogue[i];
   if(e.model==model&&e.type==t&&e.zone==z)return true;}return false;}
 bool on(int handle,const PedDecalEntry&e){std::vector<Decal>&v=decals_[handle];
  for(size_t i=0;i<v.size();++i)if(v[i].collection==e.collection&&v[i].name==e.name)return true;return false;}
 bool add(int handle,uint32_t collection,uint32_t name){const uint64_t args[3]={static_cast<uint32_t>(handle),collection,name};
  return call(source_decal::ADD_PED_DECORATION_FROM_HASHES,args,3);}
 bool pack(int handle,unsigned i){float one=1.f;uint64_t f=0;memcpy(&f,&one,sizeof(one));
  const uint64_t args[4]={static_cast<uint32_t>(handle),static_cast<uint64_t>(reinterpret_cast<uintptr_t>(PedDamagePacks[i])),f,f};
  return i<PedDamagePackCount&&call(source_decal::APPLY_PED_DAMAGE_PACK,args,4);}
 static void unavailable(Transport&m,const char*help){m.add("Unavailable",ActionRow,"");m.rows.back().enabled=false;m.description(help);}
public:
 PedOverlayPages():permitted_(false),type_(0),zone_(0){memset(&host_,0,sizeof(host_));}
 void bind(const Host&host){host_=host;permitted_=false;type_=zone_=0;decals_.clear();packs_.clear();}
 void permit(bool allowed){permitted_=allowed;}
 // Wardrobe root rows (source order: Decal Overlays, Damage Overlays, then Accessories).
 void rootRows(Transport&m,int decalTypes,int damage){
  int handle=0;uint32_t model=0;bool any=false;
  if(ped(handle,model))for(unsigned t=0;t<PedDecalTypeCount&&!any;++t)any=hasType(model,t);
  m.add("Decal Overlays",ActionRow,"",any?decalTypes:-1);if(!any){m.rows.back().enabled=false;m.description("No decal overlays available for this ped model.");}
  m.add("Damage Overlays",ActionRow,"",damage);
 }
 bool types(Transport&m,int zones){
  m.setTitle("Decal Overlays");int handle=0;uint32_t model=0;if(!ped(handle,model)){unavailable(m,"Use Back to return.");return true;}
  for(unsigned t=0;t<PedDecalTypeCount;++t)if(hasType(model,t)&&m.add(typeName(t),ActionRow,"",zones)&&m.event()==Accept)type_=t;
  bool ok=true;
  if(m.add("CLEAR ALL",ActionRow,"")&&m.event()==Accept){const uint64_t arg=static_cast<uint32_t>(handle);
   ok=call(source_decal::CLEAR_PED_DECORATIONS,&arg,1);if(ok)decals_.erase(handle);}
  m.icons(true,CrossIcon,CrossIcon);return ok;
 }
 bool zones(Transport&m,int items){
  m.setTitle(typeName(type_));int handle=0;uint32_t model=0;if(!ped(handle,model)){unavailable(m,"Use Back to return.");return true;}
  for(unsigned z=0;z<PedDecalZoneCount;++z)if(hasZone(model,type_,z)&&m.add(zoneName(z),ActionRow,"",items)&&m.event()==Accept)zone_=z;
  return true;
 }
 bool items(Transport&m){
  m.setTitle(zoneName(zone_));int handle=0;uint32_t model=0;
  if(!ped(handle,model)){unavailable(m,"Use Back to return.");return true;}bool ok=true;
  for(unsigned i=0;i<PedDecalCount;++i){const PedDecalEntry&e=PedDecalCatalogue[i];if(e.model!=model||e.type!=type_||e.zone!=zone_)continue;
   const bool applied=on(handle,e);
   if(m.add(e.caption,ActionRow,"")&&m.event()==Accept){std::vector<Decal>&v=decals_[handle];
    if(!applied){ok=add(handle,e.collection,e.name)&&ok;if(ok){Decal d={e.collection,e.name};v.push_back(d);}}
    else {for(size_t k=0;k<v.size();)if(v[k].collection==e.collection&&v[k].name==e.name)v.erase(v.begin()+k);else ++k;
     const uint64_t arg=static_cast<uint32_t>(handle);ok=call(source_decal::CLEAR_PED_DECORATIONS,&arg,1)&&ok;
     for(size_t k=0;k<v.size();++k)ok=add(handle,v[k].collection,v[k].name)&&ok;}
   }
   m.icons(on(handle,e),TickIcon,NoIcon);
  }return ok;
 }
 bool damage(Transport&m,int packs){
  m.setTitle("Damage Overlays");m.add("Damage Packs",ActionRow,"",packs);m.add("---Wash Up---",BreakRow,"");
  int handle=0;uint32_t model=0;bool ok=true;const bool known=ped(handle,model);const uint64_t arg=static_cast<uint32_t>(handle);
  if(m.add("Clear Blood Damage",ActionRow,"")&&m.event()==Accept)ok=known&&call(source_replenish::CLEAR_PED_BLOOD_DAMAGE,&arg,1);
  m.icons(true,CrossIcon,CrossIcon);
  if(m.add("Clear All Visible Damage",ActionRow,"")&&m.event()==Accept){ok=known&&call(source_replenish::RESET_PED_VISIBLE_DAMAGE,&arg,1);if(ok)packs_.erase(handle);}
  m.icons(true,CrossIcon,CrossIcon);return ok;
 }
 bool damagePacks(Transport&m){
  m.setTitle("Damage Packs");int handle=0;uint32_t model=0;if(!ped(handle,model)){unavailable(m,"Use Back to return.");return true;}
  std::vector<unsigned>&applied=packs_[handle];bool ok=true;
  for(unsigned i=0;i<PedDamagePackCount;++i){bool active=false;for(size_t k=0;k<applied.size();++k)active=active||applied[k]==i;
   if(m.add(PedDamagePacks[i],ActionRow,"")&&m.event()==Accept){
    if(!active){ok=pack(handle,i)&&ok;if(ok){applied.push_back(i);active=true;}}
    else {// source: reset visible damage, then re-apply every other tracked pack
     const uint64_t arg=static_cast<uint32_t>(handle);ok=call(source_replenish::RESET_PED_VISIBLE_DAMAGE,&arg,1)&&ok;
     for(size_t k=0;k<applied.size();)if(applied[k]==i)applied.erase(applied.begin()+k);else {ok=pack(handle,applied[k])&&ok;++k;}active=false;}
   }
   m.icons(active,TickIcon,NoIcon);
  }return ok;
 }
};
}
