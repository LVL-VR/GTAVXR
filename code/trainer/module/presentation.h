#pragma once
#include "menu_transport.h"
#include "navigation.h"
#include "theme.h"
#include <stdio.h>
namespace menyoo_vr {
// Source-native presentation plan, not a substitute font renderer. Render the
// plan with GTA's own fonts and CommonMenu/mprankbadge textures into an owned
// UI target; both VR Settings and Trainer feed exactly this same plan.
enum PrimitiveKind { RectanglePrimitive,SpritePrimitive,TextPrimitive };
struct Primitive {
 PrimitiveKind kind;float x,y,width,height,rotation,scale;Colour colour;
 uint32_t font;bool centered,rightAligned,outline;std::string text,dict,texture;
 Primitive():kind(RectanglePrimitive),x(0),y(0),width(0),height(0),rotation(0),scale(0),font(0),centered(false),rightAligned(false),outline(false){}
};
struct Presentation {
 std::vector<Primitive> commands;unsigned selected,total,offset;
 float cropLeft,cropTop,cropRight,cropBottom;
 Presentation():selected(0),total(0),offset(0),cropLeft(.058f),cropTop(.075f),cropRight(.262f),cropBottom(.20f){}
};
inline Primitive&box(Presentation&p,float x,float y,float w,float h,const Colour&colour,const char*gradient=NULL){
 p.commands.push_back(Primitive());Primitive&c=p.commands.back();c.x=x;c.y=y;c.width=w;c.height=h;c.colour=colour;
 if(gradient){c.kind=SpritePrimitive;c.dict="CommonMenu";c.texture=gradient;}return c;
}
inline Primitive&label(Presentation&p,const std::string&text,float x,float y,float scale,uint32_t font,const Colour&colour,bool center=false,bool outline=false){
 p.commands.push_back(Primitive());Primitive&c=p.commands.back();c.kind=TextPrimitive;c.text=text;c.x=x;c.y=y;c.scale=scale;c.font=font;c.colour=colour;c.centered=center;c.outline=outline;return c;
}
inline void rowIcon(Presentation&p,Icon icon,bool chosen,float y,float rotation,const Colour&colour){
 if(icon==NoIcon)return;
 Primitive&c=box(p,.247f,y+.016f,.022f,.03f,colour);c.kind=SpritePrimitive;c.rotation=rotation;
 c.dict="CommonMenu";
 switch(icon){
  case TickIcon:c.texture="shop_tick_icon";break;
  case CrossIcon:c.dict="crosstheline";c.texture="timer_largecross_32";break;
  case BoxTickIcon:c.texture=chosen?"shop_box_tickb":"shop_box_tick";break;
  case BoxCrossIcon:c.texture=chosen?"shop_box_crossb":"shop_box_cross";break;
  case BoxBlankIcon:c.texture=chosen?"shop_box_blankb":"shop_box_blank";break;
  case CarIcon:c.texture=chosen?"shop_garage_icon_b":"shop_garage_icon_a";break;
  case BikeIcon:c.texture=chosen?"shop_garage_bike_icon_b":"shop_garage_bike_icon_a";break;
  case WeaponIcon:c.texture=chosen?"shop_gunclub_icon_b":"shop_gunclub_icon_a";break;
  case TattooIcon:c.texture=chosen?"shop_tattoos_icon_b":"shop_tattoos_icon_a";break;
  case MakeupIcon:c.texture=chosen?"shop_makeup_icon_b":"shop_makeup_icon_a";break;
  case MaskIcon:c.texture=chosen?"shop_mask_icon_b":"shop_mask_icon_a";break;
  case NewStarIcon:c.texture="shop_new_star";break;
  default:c.dict="mprankbadge";c.texture="rankglobe_21x21_colour";break;
 }
}
inline Presentation present(const Transport&menu,const ThemeStyle&style,unsigned selected){
 Presentation p;if(!validStyle(style)||menu.rows.empty())return p;
 if(!repairSelection(menu.rows,selected))return p;
 p.selected=selected;p.total=static_cast<unsigned>(menu.rows.size());p.offset=windowOffset(selected,p.total);
 const unsigned visible=p.total<VisibleRows?p.total:VisibleRows;
 const bool gradient=(style.flags&Gradient)!=0;
 box(p,.16f,.1175f,.20f,.083f,style.colour[ThemeHeader],gradient?"Gradient_Nav":NULL);
 const Colour&title=style.colour[ThemeTitle];float titleScale=.75f,titleOffset=0;
 if(menu.title.size()>=15&&menu.title.size()<19){titleScale=.62f;titleOffset=.006f;}
 else if(menu.title.size()>=19&&menu.title.size()<23){titleScale=.51f;titleOffset=.011f;}
 else if(menu.title.size()>=23){titleScale=.40f;titleOffset=.015f;}
 label(p,menu.title,.16f,.10f+titleOffset,titleScale,style.font[0],title,true,title.r>240&&title.g>240&&title.b>240);
 box(p,.16f,(visible*.035f)*.5f+.159f,.20f,visible*.035f,style.colour[ThemeBackground],
     gradient&&style.colour[ThemeBackground].r<20&&style.colour[ThemeBackground].g<20&&style.colour[ThemeBackground].b<20?"Gradient_Bgd":NULL);
 for(unsigned i=0;i<visible;++i){const unsigned index=p.offset+i;const Row&r=menu.rows[index];const bool chosen=index==selected;
  const float y=(i+1)*.035f+.125f;
   if(chosen)box(p,.16f,y+.0165f,.20f,.035f,style.colour[ThemeHighlight],gradient?"Gradient_Nav":NULL);
  const Colour&colour=style.colour[r.kind==BreakRow?ThemeBreak:(chosen?ThemeSelected:ThemeOption)];
  const uint32_t font=style.font[r.kind==BreakRow?3:(chosen?2:1)];
  label(p,r.label+(r.submenu>=0?(font==2||font==7?"  ~b~==":"  ~b~>"):""),r.kind==BreakRow?.16f:.066f,y,style.font[1]==0?.33f:.40f,font,colour,r.kind==BreakRow);
  if(r.iconTrue!=NoIcon||r.iconFalse!=NoIcon){
   rowIcon(p,r.iconCondition?r.iconTrue:r.iconFalse,chosen,y,r.iconCondition?r.rotationTrue:r.rotationFalse,colour);
  }else if(r.kind==ToggleRow){
   Primitive&status=box(p,.247f,y+.0166f,.022f,.03f,Colour());
   status.kind=SpritePrimitive;status.dict="mprankbadge";status.texture="rankglobe_21x21_colour";
   const bool on=r.value=="ON"||r.value=="Enabled"||r.value=="Activado";
   const Colour enabled={102,255,102,250},disabled={255,102,102,250};status.colour=on?enabled:disabled;
  }else if(!r.value.empty()){
   // Separate right-aligned value column; numeric/choice arrows are graphics,
   // never embedded as part of a full HUD screen or a text-input prompt.
   Primitive&value=label(p,r.value,.244f,y+.0056f,.40f,font,colour);value.width=.14f;value.rightAligned=true;
   if(chosen&&(r.kind==NumberRow||r.kind==ChoiceRow)){
    Primitive&left=box(p,.205f,y+.016f,.012f,.018f,colour);left.kind=SpritePrimitive;left.dict="CommonMenu";left.texture="arrowleft";
    Primitive&right=box(p,.254f,y+.016f,.012f,.018f,colour);right.kind=SpritePrimitive;right.dict="CommonMenu";right.texture="arrowright";
   }
  }
 }
 const float footerY=(visible+1)*.035f+.1415f;
 box(p,.16f,footerY,.20f,.0345f,style.colour[ThemeHeader],gradient?"Gradient_Nav":NULL);
 if(style.flags&Separator){const Colour white={255,255,255,255};box(p,.16f,visible*.035f+.1589f,.20f,.0011f,white);}
 char count[48];sprintf_s(count,"%u / %u",selected+1,p.total);
 Primitive&counter=label(p,count,.254f,footerY-.0124f,.26f,0,style.colour[ThemeCounter]);counter.width=.18f;counter.rightAligned=true;
 if(p.total>VisibleRows){Primitive&arrows=box(p,.16f,(VisibleRows+1)*.035f+.1413f,.014f,.02f,style.colour[ThemeCounter]);arrows.kind=SpritePrimitive;arrows.dict="CommonMenu";arrows.texture="shop_arrows_upANDdown";}
 p.cropBottom=footerY+.02f;
 if(!menu.rows[selected].help.empty()){
  // Source Menu::draw_description: Arial,scale.3,outline,padding.006. Native
  // layout replaces the provisional height using actual line/character metrics
  // BEFORE crop allocation; do not guess font width from CPU/GDI glyphs.
  const float boxTop=footerY+.0345f*.5f;box(p,.16f,boxTop+.045f,.20f,.09f,style.colour[ThemeBackground]);
  Primitive&help=label(p,menu.rows[selected].help,.066f,boxTop+.006f,.3f,0,style.colour[ThemeOption],false,true);help.width=.188f;help.height=.09f;
  p.cropBottom=boxTop+.092f;
 }
 const Row&chosenRow=menu.rows[selected];
 if(chosenRow.preview){
  // Menyoo DrawVehicleBmp (0.1 x 0.0889 sprite on a 212-alpha black rectangle), placed under the menu so it
  // stays inside the owned menu target instead of to its right.
  const float h=.0889f,y=p.cropBottom+.004f+h*.5f;const Colour black={0,0,0,212},white={255,255,255,255};
  box(p,.16f,y,.103f,h+.003f,black);
  if(!chosenRow.previewDict.empty()){Primitive&s=box(p,.16f,y,.1f,h,white);s.kind=SpritePrimitive;s.dict=chosenRow.previewDict;s.texture=chosenRow.previewTexture;}
  else label(p,chosenRow.previewTexture,.16f,y-.0043f-.012f,.185f,0,white,true);
  p.cropBottom=y+h*.5f+.004f;
 }
 return p;
}
}
