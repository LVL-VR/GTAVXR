#pragma once
#include "menu_transport.h"
#include "navigation.h"
#include <map>
namespace menyoo_vr {
// Reuses Menyoo's registered submenu dispatch, with semantic filtering before
// callbacks and explicit inherited origin for shared selectors.
typedef bool (*PageCallback)(void*,Transport&);
struct PageRegistration {int id;const char*symbol;PageCallback callback;};
class PageRegistry {
 std::map<int,PageRegistration>pages_;
public:
 bool add(const PageRegistration&r){if(r.id<0||!r.symbol||!*r.symbol||!r.callback||pages_.find(r.id)!=pages_.end())return false;pages_[r.id]=r;return true;}
 const PageRegistration*find(int id)const{std::map<int,PageRegistration>::const_iterator i=pages_.find(id);return i==pages_.end()?NULL:&i->second;}
 bool allowed(int id,Origin origin)const{const PageRegistration*r=find(id);return r&&routeDecision(origin,r->symbol)==Keep;}
 bool dispatch(int id,Origin origin,void*context,Transport&menu,unsigned selected,Event event)const{
  const PageRegistration*r=find(id);if(!r||routeDecision(origin,r->symbol)!=Keep)return false;
  menu.begin(origin,pageSection(origin,r->symbol),selected,event);return r->callback(context,menu);
 }
};
struct PageLocation {int page;Origin origin;unsigned selected;};
class PageStack {
 PageLocation values_[HistoryCapacity];unsigned count_;PageLocation current_;
public:
 PageStack():count_(0){current_.page=0;current_.origin=Root;current_.selected=0;}
 const PageLocation&current()const{return current_;}
 void select(unsigned selected){current_.selected=selected;}
 void reset(int root){count_=0;current_.page=root;current_.origin=Root;current_.selected=0;}
 bool enter(int id,const PageRegistry&registry){
  const PageRegistration*r=registry.find(id);if(!r||count_==HistoryCapacity)return false;
  const Origin origin=current_.origin==Root?rootOrigin(r->symbol,Root):current_.origin;
  if(!registry.allowed(id,origin))return false;
  values_[count_++]=current_;current_.page=id;current_.origin=origin;current_.selected=0;return true;
 }
 bool back(){if(!count_)return false;current_=values_[--count_];return true;}
};
}
