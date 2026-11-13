#pragma once
#include "pugixml.hpp"
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <vector>
#include <string>
#include <set>
namespace menyoo_vr {
// Same FavouritePeds/Ped(hash,customName) schema as Menyoo. No custom-name
// keyboard: a controller-selected model supplies its installed native name.
// Pugixml is the EXACT submodule revision pinned by the retained Menyoo fork.
class ModelFavourites {
public:
 struct Entry{uint32_t hash;std::string name;};
private:
 pugi::xml_document doc_;char path_[512];bool loaded_;unsigned counter_;
 std::vector<Entry>cached_;std::set<uint32_t>hashes_;
 static bool modelHash(const char*text,uint32_t&value){
  if(!text||!*text||*text=='-'||*text=='+')return false;char*end=NULL;errno=0;const unsigned long n=strtoul(text,&end,0);
  if(errno==ERANGE||!end||*end||!n)return false;value=static_cast<uint32_t>(n);return true;
 }
 static pugi::xml_node find(pugi::xml_document&doc,uint32_t hash){
  for(pugi::xml_node node=doc.document_element().child("Ped");node;node=node.next_sibling("Ped")){
   uint32_t stored=0;if(modelHash(node.attribute("hash").value(),stored)&&stored==hash)return node;
  }return pugi::xml_node();
 }
 void adopt(pugi::xml_document&doc){
  std::vector<Entry>next;std::set<uint32_t>hashes;
  for(pugi::xml_node node=doc.document_element().child("Ped");node&&next.size()<8192;node=node.next_sibling("Ped")){
   uint32_t hash=0;if(!modelHash(node.attribute("hash").value(),hash)||!hashes.insert(hash).second)continue;
   Entry row;row.hash=hash;row.name=node.attribute("customName").value();next.push_back(row);
  }doc_.reset(doc);cached_.swap(next);hashes_.swap(hashes);loaded_=true;
 }
 static bool read(const char*path,pugi::xml_document&doc){
  WIN32_FILE_ATTRIBUTE_DATA data={};
  if(!GetFileAttributesExA(path,GetFileExInfoStandard,&data)){
   const DWORD error=GetLastError();if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND)return false;
   doc.append_child("FavouritePeds");return true;
  }
  if((data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||data.nFileSizeHigh||data.nFileSizeLow>1024*1024)return false;
  if(!doc.load_file(path,pugi::parse_default|pugi::parse_declaration))return false;
  const char*root=doc.document_element().name();
  // Original AddPedToFavourites accidentally created FavouriteWeapons. Accept
  // that historical spelling, never overwrite an unrelated XML document.
  return !strcmp(root,"FavouritePeds")||!strcmp(root,"FavouriteWeapons");
 }
 struct Writer:pugi::xml_writer{
  HANDLE file;bool good;size_t total;explicit Writer(HANDLE handle):file(handle),good(true),total(0){}
  void write(const void*data,size_t size){
   const char*bytes=static_cast<const char*>(data);
   if(size>1024*1024-total){good=false;return;}total+=size;
   while(good&&size){const DWORD n=static_cast<DWORD>(size>0x7fffffffu?0x7fffffffu:size);DWORD wrote=0;
    good=WriteFile(file,bytes,n,&wrote,NULL)!=0&&wrote==n;bytes+=wrote;size-=wrote;}
  }
 };
 bool save(pugi::xml_document&doc){
  pugi::xml_node declaration=doc.first_child();
  if(declaration.type()!=pugi::node_declaration){declaration=doc.prepend_child(pugi::node_declaration);declaration.append_attribute("version")="1.0";}
  pugi::xml_attribute encoding=declaration.attribute("encoding");if(!encoding)encoding=declaration.append_attribute("encoding");encoding="UTF-8";
  char temporary[576];if(sprintf_s(temporary,"%s.tmp-%lu-%u",path_,GetCurrentProcessId(),++counter_)<0)return false;
  HANDLE file=CreateFileA(temporary,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
  if(file==INVALID_HANDLE_VALUE)return false;Writer writer(file);doc.save(writer,"  ",pugi::format_default,pugi::encoding_utf8);
  const bool flushed=writer.good&&FlushFileBuffers(file)!=0;const bool closed=CloseHandle(file)!=0;
  const bool committed=flushed&&closed&&MoveFileExA(temporary,path_,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
  if(!committed)DeleteFileA(temporary); // only OUR exclusive, newly created temporary
  return committed;
 }
public:
 ModelFavourites():loaded_(false),counter_(0){path_[0]=0;}
 bool configure(const char*settings){
  loaded_=false;doc_.reset();cached_.clear();hashes_.clear();path_[0]=0;
  if(!settings||strlen(settings)>=sizeof(path_))return false;strcpy_s(path_,settings);
  char*slash=strrchr(path_,'/'),*backslash=strrchr(path_,'\\');if(backslash&&(!slash||backslash>slash))slash=backslash;
  if(!slash)return false;const size_t prefix=static_cast<size_t>(slash-path_)+1;
  if(prefix+strlen("FavouritePeds.xml")>=sizeof(path_))return false;strcpy_s(path_+prefix,sizeof(path_)-prefix,"FavouritePeds.xml");
  return reload();
 }
 bool reload(){pugi::xml_document next;if(!path_[0]||!read(path_,next))return false;adopt(next);return true;}
 bool contains(uint32_t hash)const{return loaded_&&hashes_.find(hash)!=hashes_.end();}
 const std::vector<Entry>&entries()const{return cached_;}
 // Re-read ONLY at explicit edit: preserve unrelated changes made since menu
 // load. No disk/file read per frame or during native render presentation.
 bool toggle(uint32_t hash,const char*name){
  if(!hash||!name||!*name||strlen(name)>=64||!path_[0])return false;pugi::xml_document next;if(!read(path_,next))return false;
  pugi::xml_node found=find(next,hash);
  if(found){do{found.parent().remove_child(found);found=find(next,hash);}while(found);}
  else {char hex[16];sprintf_s(hex,"0x%08X",hash);pugi::xml_node node=next.document_element().append_child("Ped");
   node.append_attribute("hash")=hex;node.append_attribute("customName")=name;}
  if(!save(next))return false;adopt(next);return true;
 }
 bool loaded()const{return loaded_;}
};
}
