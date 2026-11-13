#pragma once
#include "ped_actions.h"
#include "menu_transport.h"
namespace menyoo_vr {
// Original component/prop ordering and controller steppers. No camera takeover,
// keyboard, saved outfit, beta outfit or Head Features paths are present.
class WardrobePages {
 PedActions*ped_;unsigned selected_;bool prop_;int confirmation_;uint64_t until_;
 bool utility(Transport&m,const char*label,unsigned action,uint64_t now){
  const bool chosen=m.add(label,ActionRow,"");
  if(confirmation_==static_cast<int>(action)&&now<=until_)m.description("Press again to confirm.");
  if(!chosen||m.event()!=Accept)return true;
  if(confirmation_==static_cast<int>(action)&&now<=until_){confirmation_=-1;return ped_->utility(action);}
  confirmation_=static_cast<int>(action);until_=now+2500;m.description("Press again to confirm.");return true;
 }
 static int step(int value,int lo,int hi,Event event){return event==Right?(value<hi?value+1:lo):(value>lo?value-1:hi);}
 static void unavailable(Transport&m){m.add("Unavailable",ActionRow,"");m.description("Use Back to return.");}
public:
 WardrobePages():ped_(NULL),selected_(0),prop_(false),confirmation_(-1),until_(0){}
 void bind(PedActions&ped){ped_=&ped;selected_=0;prop_=false;confirmation_=-1;until_=0;}
 void clearConfirmation(){confirmation_=-1;until_=0;}
 // overlayRows adds the retained Decal/Damage Overlays rows in the source position (before Accessories).
 bool root(Transport&m,int accessories,int editor,uint64_t now,void(*overlayRows)(Transport&)=NULL){
  m.setTitle("Wardrobe");bool ok=true;if(overlayRows)overlayRows(m);m.add("Accessories",ActionRow,"",accessories);m.add("---Components---",BreakRow,"");
  const char*names[]={"Head [head]","Beard/Mask [berd]","Hair [hair]","Torso [uppr]","Legs [lowr]","Hands/Back [hand]",
   "Shoes [feet]","Teeth/Scarf/Necklace/Bracelets [teef]","Accessory/Tops [accs]","Task/Armour [task]","Emblem [decl]","Tops2 (Outer) [jbib]"};
  for(unsigned i=0;i<12;++i){int count=0;if(!ped_->query(source_ped::GET_NUMBER_OF_PED_DRAWABLE_VARIATIONS,i,count))continue;
   if(count>0&&m.add(names[i],ActionRow,"",editor)&&m.event()==Accept){selected_=i;prop_=false;}}
  m.add("---Utilities---",BreakRow,"");
  ok=utility(m,"Random Components",0,now)&&ok;ok=utility(m,"Default Components",1,now)&&ok;ok=utility(m,"Default Components and Accessories",2,now)&&ok;
  return ok;
 }
 bool accessories(Transport&m,int editor,uint64_t now){
  m.setTitle("Accessories");const char*names[]={"Hats [p_head]","Glasses [p_eyes]","Ear Pieces [p_ears]","Unknown 3","Unknown 4","Unknown 5","Watches [p_lwrist]","Bangles [p_rwrist]","Unknown 8","Unknown 9"};
  for(unsigned i=0;i<10;++i){int count=0;if(!ped_->query(source_ped::GET_NUMBER_OF_PED_PROP_DRAWABLE_VARIATIONS,i,count))continue;
   if(count>0&&m.add(names[i],ActionRow,"",editor)&&m.event()==Accept){selected_=i;prop_=true;}}
  m.add("---Utilities---",BreakRow,"");bool ok=utility(m,"Random Accessories",3,now);ok=utility(m,"Default Accessories",4,now)&&ok;
  return ok;
 }
 bool editor(Transport&m){
  m.setTitle("Set Variation");int drawable=0,texture=0,palette=0,count=0,textures=0;
  if(!ped_->query(prop_?source_ped::GET_PED_PROP_INDEX:source_ped::GET_PED_DRAWABLE_VARIATION,selected_,drawable)||
     !ped_->query(prop_?source_ped::GET_NUMBER_OF_PED_PROP_DRAWABLE_VARIATIONS:source_ped::GET_NUMBER_OF_PED_DRAWABLE_VARIATIONS,selected_,count)||count<=0||count>65536){unavailable(m);return true;}
  const int previousDrawable=drawable;const int minimum=prop_?-1:0;
  if(drawable<minimum||drawable>=count){unavailable(m);return true;}
  if(drawable>=0&&!ped_->query(prop_?source_ped::GET_PED_PROP_TEXTURE_INDEX:source_ped::GET_PED_TEXTURE_VARIATION,selected_,texture)){unavailable(m);return true;}
  const int previousTexture=texture;
  if(!prop_&&!ped_->query(source_ped::GET_PED_PALETTE_VARIATION,selected_,palette)){unavailable(m);return true;}
  char formatted[48];sprintf_s(formatted,"%d",drawable);
  if(m.add("Type",NumberRow,formatted)&&(m.event()==Left||m.event()==Right))drawable=step(drawable,minimum,count-1,m.event());
  if(drawable!=previousDrawable)texture=0;
  if(drawable>=0){if(!ped_->query(prop_?source_ped::GET_NUMBER_OF_PED_PROP_TEXTURE_VARIATIONS:source_ped::GET_NUMBER_OF_PED_TEXTURE_VARIATIONS,selected_,textures,drawable)||textures<=0||textures>65536){unavailable(m);return true;}
   if(texture<0||texture>=textures)texture=0;sprintf_s(formatted,"%d",texture);
   if(m.add("Texture",NumberRow,formatted)&&(m.event()==Left||m.event()==Right))texture=step(texture,0,textures-1,m.event());}
  // Menyoo "---Collection Data---" (Collection cycler + Local ID stepper) from SourceHostVariationInfo runs.
  std::vector<VariationCollection>runs;
  if(drawable>=0&&ped_->collections(prop_,selected_,runs)&&!runs.empty()){
   int current=0;for(size_t i=0;i<runs.size();++i)if(static_cast<uint32_t>(previousDrawable)>=runs[i].first&&static_cast<uint32_t>(previousDrawable)<runs[i].first+runs[i].count)current=static_cast<int>(i);
   const VariationCollection&col=runs[current];const int local=previousDrawable-static_cast<int>(col.first);
   m.add("---Collection Data---",BreakRow,"");
   if(m.add("Collection",ChoiceRow,col.name)&&(m.event()==Left||m.event()==Right)){
    const int n=static_cast<int>(runs.size());const int next=(current+(m.event()==Right?1:n-1))%n;drawable=static_cast<int>(runs[next].first);texture=0;}
   m.description("The clothing set this item was added in (base game, a game update or a mod). Base game items are listed as \"basegame\".");
   char localText[24];sprintf_s(localText,"%d",local);
   if(m.add("Local ID",NumberRow,localText)&&(m.event()==Left||m.event()==Right)&&col.count){
    drawable=static_cast<int>(col.first)+step(local,0,static_cast<int>(col.count)-1,m.event());texture=0;}
   m.description("The item's number within its collection. Unlike Type, it stays the same after game updates.");
  }
  // Editing Type/Texture does not fire on a NoEvent redraw. Native variation
  // streams normally; no busy wait or forced frame/simulation advancement.
  if((m.event()!=Left&&m.event()!=Right)||(drawable==previousDrawable&&texture==previousTexture))return true;
  return prop_?ped_->prop(selected_,drawable,texture):ped_->component(selected_,drawable,texture,palette);
 }
};
}
