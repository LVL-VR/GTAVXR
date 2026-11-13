#pragma once
#include "appearance.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
namespace menyoo_vr {
inline bool decodeColours(const char*text,Appearance&appearance){
 if(!text||!validAppearance(appearance))return false;Appearance decoded=appearance;
 const char*position=text;
 for(unsigned role=0;role<ColourRoleCount;++role){uint32_t*const channels[]={&decoded.colour[role].r,&decoded.colour[role].g,&decoded.colour[role].b,&decoded.colour[role].a};
  for(unsigned channel=0;channel<4;++channel){if(*position<'0'||*position>'9')return false;unsigned value=0,digits=0;
   while(*position>='0'&&*position<='9'){if(++digits>3)return false;value=value*10+unsigned(*position++-'0');}
   if(value>255)return false;*channels[channel]=value;
   const bool last=role+1==ColourRoleCount&&channel==3;if(last){if(*position)return false;}else if(*position++!=',')return false;
  }
 }
 appearance=decoded;return true;
}
inline bool encodeColours(const Appearance&appearance,char*text,size_t capacity){
 if(!text||!capacity||!validAppearance(appearance))return false;text[0]=0;size_t offset=0;
 for(unsigned role=0;role<ColourRoleCount;++role){const Colour&colour=appearance.colour[role];const uint32_t channels[]={colour.r,colour.g,colour.b,colour.a};
  for(unsigned channel=0;channel<4;++channel){const int count=_snprintf_s(text+offset,capacity-offset,_TRUNCATE,"%s%u",offset?",":"",channels[channel]);
   if(count<0)return false;offset+=size_t(count);}
 }return true;
}
inline Appearance loadAppearance(const char*path){
 Appearance appearance=defaultAppearance();if(!path||!*path)return appearance;
 // Called on settings initialization only, never for row/frame presentation.
 const unsigned language=GetPrivateProfileIntA("MenuAppearance","language",English,path);
 if(language<=Spanish)appearance.language=language;
 char colours[192]={};GetPrivateProfileStringA("MenuAppearance","colours","",colours,sizeof(colours),path);
 if(colours[0])decodeColours(colours,appearance);return appearance;
}
inline bool saveAppearance(const char*path,const Appearance&appearance){
 if(!path||!*path||!validAppearance(appearance))return false;char colours[192]={},language[12]={};
 if(!encodeColours(appearance,colours,sizeof(colours)))return false;sprintf_s(language,"%u",appearance.language);
 // Dedicated section only; VR calibrations and gameplay/cheat settings intact.
 return WritePrivateProfileStringA("MenuAppearance","colours",colours,path)!=0&&
        WritePrivateProfileStringA("MenuAppearance","language",language,path)!=0;
}
}
