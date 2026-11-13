#pragma once
#include "theme.h"
#include <windows.h>
#include <stdio.h>
namespace menyoo_vr {
// Dedicated section: never clears/replaces the user's existing calibrations,
// campaign data, language or native gameplay settings. Load on initialization,
// save on edits, no configuration reads in menu drawing or per-frame updates.
inline bool parseThemeUnsigned(const char*text,unsigned limit,uint32_t&value){
 if(!text||!*text)return false;unsigned parsed=0;
 for(const char*p=text;*p;++p){if(*p<'0'||*p>'9')return false;const unsigned digit=unsigned(*p-'0');if(digit>limit||parsed>(limit-digit)/10u)return false;parsed=parsed*10+digit;}
 value=parsed;return true;
}
inline bool readThemeValue(const char*path,const char*key,unsigned limit,uint32_t&value){
 char text[32]={};const DWORD length=GetPrivateProfileStringA("MenuTheme",key,"",text,sizeof(text),path);
 return length<sizeof(text)-1&&parseThemeUnsigned(text,limit,value);
}
inline ThemeStyle loadTheme(const char*path){
 ThemeStyle style=defaultStyle();if(!path||!*path)return style;
 uint32_t version=0;if(!readThemeValue(path,"version",1,version)||version!=1)return style;
 ThemeStyle decoded={};if(!readThemeValue(path,"flags",7,decoded.flags))return style;
 for(unsigned role=0;role<ThemeColourCount;++role){uint32_t*values[]={&decoded.colour[role].r,&decoded.colour[role].g,&decoded.colour[role].b,&decoded.colour[role].a};
  for(unsigned c=0;c<4;++c){char key[40];sprintf_s(key,"colour_%u_%u",role,c);if(!readThemeValue(path,key,255,*values[c]))return style;}}
 for(unsigned i=0;i<6;++i){char key[40];sprintf_s(key,"font_%u",i);if(!readThemeValue(path,key,7,decoded.font[i]))return style;}
 return validStyle(decoded)?decoded:style;
}
inline bool saveTheme(const char*path,const ThemeStyle&style){
 if(!path||!*path||!validStyle(style))return false;char value[24];
 sprintf_s(value,"%u",style.flags);if(!WritePrivateProfileStringA("MenuTheme","flags",value,path))return false;
 for(unsigned role=0;role<ThemeColourCount;++role){const Colour&colour=style.colour[role];const uint32_t values[]={colour.r,colour.g,colour.b,colour.a};
  for(unsigned c=0;c<4;++c){char key[40];sprintf_s(key,"colour_%u_%u",role,c);sprintf_s(value,"%u",values[c]);if(!WritePrivateProfileStringA("MenuTheme",key,value,path))return false;}}
 for(unsigned i=0;i<6;++i){char key[40];sprintf_s(key,"font_%u",i);sprintf_s(value,"%u",style.font[i]);if(!WritePrivateProfileStringA("MenuTheme",key,value,path))return false;}
 return WritePrivateProfileStringA("MenuTheme","version","1",path)!=0;
}
}
