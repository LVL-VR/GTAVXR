#pragma once
#include "host_abi.h"
#include "character_host_abi.h"
#include <vector>
#include <map>
#include <string.h>
namespace menyoo_vr {
class CharacterActions {
 Host host_;bool permitted_,loaded_;std::vector<CatalogueEntry>peds_;
 std::vector<unsigned>groups_[14];std::map<uint32_t,unsigned>byHash_;
 int call(uint64_t operation,const uint64_t*args,unsigned count){
  if(!permitted_||!host_.invoke)return WrongContext;uint64_t result[3]={};return host_.invoke(host_.user,operation,args,count,result);
 }
public:
 CharacterActions():permitted_(false),loaded_(false){memset(&host_,0,sizeof(host_));}
 void bind(const Host&host){host_=host;permitted_=loaded_=false;peds_.clear();byHash_.clear();for(unsigned i=0;i<14;++i)groups_[i].clear();}
 void permit(bool enabled){permitted_=enabled;}
 bool load(){
  if(loaded_)return true;std::vector<CatalogueEntry>next;
  for(unsigned i=0;;++i){CatalogueEntry row={};row.size=sizeof(row);
   uint64_t args[3]={CataloguePed,i,reinterpret_cast<uintptr_t>(&row)};
   if(call(SourceHostCatalogueRead,args,3)!=Success||row.size!=sizeof(row)||row.total>8192)return false;
   if(!row.total){loaded_=true;peds_.clear();byHash_.clear();for(unsigned n=0;n<14;++n)groups_[n].clear();return true;}
   if(i>=row.total||(!next.empty()&&row.total!=next[0].total)||!row.hash||row.category>=10||!row.name[0]||!memchr(row.name,0,sizeof(row.name)))return false;
   next.push_back(row);if(i+1==row.total)break;
  }
  std::vector<unsigned>nextGroups[14];std::map<uint32_t,unsigned>nextHashes;
  for(unsigned i=0;i<next.size();++i){nextGroups[group(next[i])].push_back(i);nextHashes[next[i].hash]=i;}
  peds_.swap(next);for(unsigned i=0;i<14;++i)groups_[i].swap(nextGroups[i]);byHash_.swap(nextHashes);loaded_=true;return true;
 }
 static bool prefix(const char*name,const char*pattern){return !strncmp(name,pattern,strlen(pattern));}
 static unsigned group(const CatalogueEntry&entry){const char*name=entry.name;
  if(entry.category==8)return 4; // native animal metadata, not model-name guessing
  const char*patterns[]={"player_","ig_","cs_","mp_","", "a_f_","a_m_","g_f_","g_m_","s_f_","s_m_","u_f_","u_m_"};
  if(prefix(name,"csb_"))return 2;
  for(unsigned i=0;i<13;++i)if(patterns[i][0]&&prefix(name,patterns[i]))return i;
  return 13;
 }
 unsigned count(unsigned category)const{return category<14?static_cast<unsigned>(groups_[category].size()):0;}
 const CatalogueEntry*entry(unsigned category,unsigned index)const{return category<14&&index<groups_[category].size()?&peds_[groups_[category][index]]:NULL;}
 const CatalogueEntry*byHash(uint32_t hash)const{std::map<uint32_t,unsigned>::const_iterator found=byHash_.find(hash);return found==byHash_.end()?NULL:&peds_[found->second];}
 int request(unsigned category,unsigned index){const CatalogueEntry*row=entry(category,index);if(!row)return InvalidInput;
  uint64_t args[2]={row->hash,reinterpret_cast<uintptr_t>(row->name)};return call(SourceHostPlayerModelRequest,args,2);}
 int restore(){return call(SourceHostRestoreStory,NULL,0);}
 int switchStory(unsigned target){if(target>=3)return InvalidInput;uint64_t arg=target;return call(SourceHostSwitchStory,&arg,1);}
 void refresh(){loaded_=false;}
};
}
