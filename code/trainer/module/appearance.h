#pragma once
#include <stdint.h>
namespace menyoo_vr {
enum LanguageId { English=0,Spanish=1 };
enum ColourRole { Header,Background,TitleText,OptionText,SelectedText,Selection,HelpText,ColourRoleCount };
struct Colour {uint32_t r,g,b,a;};
struct Appearance {uint32_t size,version,language,reserved;Colour colour[ColourRoleCount];};
inline Appearance defaultAppearance(){
 Appearance a={};a.size=sizeof(a);a.version=1;a.language=English;
 // Original Menu.cpp palette; RGB and individual alpha remain editable.
 const Colour palette[ColourRoleCount]={{0,255,255,247},{10,10,10,200},{255,255,255,255},
  {255,255,255,255},{0,0,0,255},{255,255,255,211},{255,255,255,255}};
 for(unsigned i=0;i<ColourRoleCount;++i)a.colour[i]=palette[i];return a;
}
inline bool validAppearance(const Appearance&a){
 if(a.size!=sizeof(a)||a.version!=1||a.language>Spanish||a.reserved)return false;
 for(unsigned i=0;i<ColourRoleCount;++i){const Colour&c=a.colour[i];if(c.r>255||c.g>255||c.b>255||c.a>255)return false;}return true;
}
}
