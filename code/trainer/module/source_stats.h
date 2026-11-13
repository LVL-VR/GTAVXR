#pragma once
#include "host_abi.h"
#include "source_stat_commands.h"
#include <string>
#include <string.h>
namespace menyoo_vr {
class SourceStats {
 Host host_;bool permitted_;int status_;
 bool call(uint64_t hash,const uint64_t*args,unsigned count,uint64_t result[3]){
  memset(result,0,sizeof(uint64_t)*3);
  if(!permitted_||!host_.invoke){status_=WrongContext;return false;}
  status_=host_.invoke(host_.user,hash,args,count,result);return status_==Success;
 }
 bool key(const std::string&name,uint64_t&hash){
  const char*text=name.c_str();uint64_t args[1]={},result[3]={};memcpy(&args[0],&text,sizeof(text));
  if(!call(source_stats::GET_HASH_KEY,args,1,result))return false;hash=result[0]&UINT64_C(0xffffffff);return true;
 }
 template<class T>bool set(uint64_t command,const std::string&name,const T&value){
  uint64_t args[3]={},result[3]={};if(!key(name,args[0]))return false;
  memcpy(&args[1],&value,sizeof(value));args[2]=0; // native coderAssert, NOT an SDK save flag
  return call(command,args,3,result)&&result[0]!=0;
 }
public:
 SourceStats():permitted_(false),status_(WrongContext){memset(&host_,0,sizeof(host_));}
 bool bind(const Host&host){if(host.size!=sizeof(host)||host.version!=HostAbiVersion||!host.invoke)return false;host_=host;return true;}
 void permit(bool value){permitted_=value;}
 int status()const{return status_;}
 bool getInt(const std::string&name,int&value){
  uint64_t args[3]={},result[3]={};if(!key(name,args[0]))return false;
  int temporary=0;int*pointer=&temporary;memcpy(&args[1],&pointer,sizeof(pointer));args[2]=uint32_t(-1);
  if(!call(source_stats::STAT_GET_INT,args,3,result)||!result[0])return false;value=temporary;return true;
 }
 bool setInt(const std::string&name,int value){return set(source_stats::STAT_SET_INT,name,value);}
 bool setFloat(const std::string&name,float value){return set(source_stats::STAT_SET_FLOAT,name,value);}
 bool setBool(const std::string&name,bool value){return set(source_stats::STAT_SET_BOOL,name,value);}
 bool increment(const std::string&name,float value){uint64_t args[2]={},result[3]={};if(!key(name,args[0]))return false;memcpy(&args[1],&value,sizeof(value));return call(source_stats::STAT_INCREMENT,args,2,result);}
 bool autoSave(){uint64_t result[3]={};return call(source_stats::DO_AUTO_SAVE,NULL,0,result);}
};
}
