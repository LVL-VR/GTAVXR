#pragma once
// Separate versioned extension: existing300-byte calibration ABI is unchanged.
#include "ui_element_policy.h"
#pragma pack(push,8)
struct GxUiVisibility {uint32_t size,version,mask;int32_t pausePercent;};
#pragma pack(pop)
static_assert(sizeof(GxUiVisibility)==16,"UI visibility extension ABI");
typedef int (GX_CALL *GxSetUiVisibility)(void*,const GxUiVisibility*);
namespace gx_ui_visibility {
enum {Map=gx_ui_element::Count,Notices=Map+1,Count=Notices+1,All=(1u<<Count)-1};
inline GxUiVisibility defaults(){GxUiVisibility p={sizeof(GxUiVisibility),1,All,100};return p;}
inline bool valid(const GxUiVisibility&p){return p.size==sizeof(p)&&p.version==1&&!(p.mask&~All)&&p.pausePercent>=40&&p.pausePercent<=200;}
inline bool enabled(const GxUiVisibility&p,unsigned kind){return kind<Count&&(p.mask&(1u<<kind))!=0;}
inline void toggle(GxUiVisibility&p,unsigned kind){if(kind<Count)p.mask^=1u<<kind;}
inline bool product(const GxUiVisibility&p,const GxUiItem&r){
    if(r.route==GX_UI_MENU_PANEL||gx_native_ui::handRoute(r.route))return true;
    if(gx_native_ui::wantedItem(r))return enabled(p,gx_ui_element::Wanted);
    if(r.route==GX_UI_PANEL&&r.id!=15)return enabled(p,Notices);
    return enabled(p,gx_ui_element::kind(r));
}
}
