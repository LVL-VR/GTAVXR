#pragma once
#include "presentation.h"
#include "presentation_abi.h"
#include <string.h>
namespace menyoo_vr {
inline bool drawString(char*target,unsigned capacity,const std::string&source){
 if(source.size()>=capacity||source.find('\0')!=std::string::npos)return false;
 memcpy(target,source.c_str(),source.size()+1);return true;
}
inline bool encodePresentation(const Presentation&p,uint64_t generation,DrawFrame&frame){
 if(p.commands.size()>MaxPrimitives||!generation)return false;
 DrawFrame result={};result.size=sizeof(result);result.version=PresentationVersion;result.generation=generation;
 result.selected=p.selected;result.total=p.total;result.offset=p.offset;result.count=static_cast<uint32_t>(p.commands.size());
 result.cropLeft=p.cropLeft;result.cropTop=p.cropTop;result.cropRight=p.cropRight;result.cropBottom=p.cropBottom;
 for(unsigned i=0;i<result.count;++i){const Primitive&c=p.commands[i];DrawPrimitive&d=result.commands[i];
  d.kind=c.kind;d.font=c.font;d.red=c.colour.r;d.green=c.colour.g;d.blue=c.colour.b;d.alpha=c.colour.a;
  d.centered=c.centered;d.rightAligned=c.rightAligned;d.outline=c.outline;
  d.x=c.x;d.y=c.y;d.width=c.width;d.height=c.height;d.rotation=c.rotation;d.scale=c.scale;
  if(!drawString(d.text,sizeof(d.text),c.text)||!drawString(d.dictionary,sizeof(d.dictionary),c.dict)||!drawString(d.texture,sizeof(d.texture),c.texture))return false;
 }
 if(!validDrawFrame(result))return false;frame=result;return true;
}
}
