#pragma once
#include "host_abi.h"
#include "character_host_abi.h"
#include "vehicle_host_abi.h"
#include <vector>
#include <map>
#include <string.h>
namespace menyoo_vr {
// Installed native catalog replaces Menyoo's retail model constants, retaining
// vehicle-class navigation. Shared source catalog already serves Player/Weapons.
class VehicleActions {
 Host host_;bool permitted_,loaded_;std::vector<CatalogueEntry>vehicles_;std::vector<unsigned>groups_[23];
 int call(uint64_t op,const uint64_t*args,unsigned count){
  if(!permitted_||!host_.invoke)return WrongContext;uint64_t result[3]={};return host_.invoke(host_.user,op,args,count,result);
 }
public:
 VehicleActions():permitted_(false),loaded_(false){memset(&host_,0,sizeof(host_));}
 void bind(const Host&host){host_=host;permitted_=loaded_=false;vehicles_.clear();for(unsigned i=0;i<23;++i)groups_[i].clear();}
 void permit(bool enabled){permitted_=enabled;}
 bool load(){
  if(loaded_)return true;std::vector<CatalogueEntry>next;std::map<uint32_t,bool>seen;
  for(unsigned i=0;;++i){CatalogueEntry row={};row.size=sizeof(row);uint64_t args[3]={CatalogueVehicle,i,reinterpret_cast<uintptr_t>(&row)};
   if(call(SourceHostCatalogueRead,args,3)!=Success||row.size!=sizeof(row)||row.total>2048)return false;
   if(!row.total){vehicles_.clear();for(unsigned n=0;n<23;++n)groups_[n].clear();loaded_=true;return true;}
   if(i>=row.total||(!next.empty()&&row.total!=next[0].total)||!row.hash||row.category>=23||!row.name[0]||!memchr(row.name,0,sizeof(row.name))||seen[row.hash])return false;
   seen[row.hash]=true;next.push_back(row);if(i+1==row.total)break;
  }
  std::vector<unsigned>groups[23];for(unsigned i=0;i<next.size();++i)groups[next[i].category].push_back(i);
  vehicles_.swap(next);for(unsigned i=0;i<23;++i)groups_[i].swap(groups[i]);loaded_=true;return true;
 }
 unsigned count(unsigned group)const{return group<23?static_cast<unsigned>(groups_[group].size()):0;}
 const CatalogueEntry*entry(unsigned group,unsigned index)const{return group<23&&index<groups_[group].size()?&vehicles_[groups_[group][index]]:NULL;}
 int request(unsigned group,unsigned index){const CatalogueEntry*row=entry(group,index);if(!row)return InvalidInput;
  uint64_t args[2]={row->hash,reinterpret_cast<uintptr_t>(row->name)};return call(SourceHostVehicleSpawnRequest,args,2);}
 void refresh(){loaded_=false;}
 static const char*categoryName(unsigned group){const char*names[]={"Compacts","Sedans","SUVs","Coupes","Muscle","Sports Classics","Sports","Super",
  "Motorcycles","Off-Road","Industrial","Utility","Vans","Cycles","Boats","Helicopters","Planes","Service","Emergency","Military","Commercial","Trains","Open Wheel"};
  return group<23?names[group]:"Others";}
};
}
