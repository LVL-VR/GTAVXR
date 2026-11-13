#pragma once
#include "menu_transport.h"
#include <stdint.h>
// Menyoo PC navigation contract (MAFINS, GPL-3.0-or-later):
// Menu.cpp AddOption/AddBreak use GTA_MAXOP=14, GTA_SCROLLOP=8,
// GTA_BETOP=6; Up/Down skip breaks, with per-page selection history.
// Pure policy shared by the VR menu and trainer, no game/OS input injection.
namespace menyoo_vr {
enum { VisibleRows=14, HistoryCapacity=100 };
inline unsigned windowOffset(unsigned selected,unsigned count){
 if(count<=VisibleRows)return 0;
 if(selected>=count)selected=count-1;
 const unsigned proposed=selected>=7?selected-7:0;
 return proposed<count-VisibleRows?proposed:count-VisibleRows;
}
inline bool selectable(const Row&r){return r.enabled&&r.kind!=BreakRow;}
inline bool moveSelection(const std::vector<Row>&rows,unsigned&selected,int direction){
 if(rows.empty()||!direction)return false;
 const unsigned count=static_cast<unsigned>(rows.size());
 unsigned current=selected<count?selected:0;
 for(unsigned i=0;i<count;++i){
  current=direction>0?(current+1==count?0:current+1):(current?current-1:count-1);
  if(selectable(rows[current])){selected=current;return true;}
 }
 return false;
}
inline bool repairSelection(const std::vector<Row>&rows,unsigned&selected){
 if(rows.empty()){selected=0;return false;}
 if(selected>=rows.size())selected=static_cast<unsigned>(rows.size()-1);
 if(selectable(rows[selected]))return true;
 return moveSelection(rows,selected,1);
}
class History {
 struct Entry {int page;unsigned selection;};
 Entry entries_[HistoryCapacity];unsigned depth_;
public:
 History():depth_(0){}
 void clear(){depth_=0;}
 unsigned depth()const{return depth_;}
 bool push(int page,unsigned selection){
  if(page<0||depth_==HistoryCapacity)return false;
  entries_[depth_].page=page;entries_[depth_].selection=selection;++depth_;return true;
 }
 bool pop(int&page,unsigned&selection){
  if(!depth_)return false;
  --depth_;page=entries_[depth_].page;selection=entries_[depth_].selection;return true;
 }
};
// Upstream starts held-arrow repetition after630ms, then repeats each update.
// Accept/back remain edge-triggered so holding
// A cannot activate multiple cheats or accidentally drill through submenus.
class Repeat {
 unsigned held_;uint64_t started_,last_;bool initialized_;
public:
 Repeat():held_(0),started_(0),last_(0),initialized_(false){}
 void reset(){held_=0;started_=last_=0;initialized_=false;}
 bool poll(unsigned direction,uint64_t milliseconds){
  if(!direction){reset();return false;}
  if(!initialized_||direction!=held_||milliseconds<last_){
   held_=direction;started_=last_=milliseconds;initialized_=true;return true;
  }
  if(milliseconds-started_<=630||milliseconds==last_)return false;
  last_=milliseconds;return true;
 }
};
}
