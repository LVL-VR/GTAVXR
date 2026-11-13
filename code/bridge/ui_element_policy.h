#pragma once
// Pure bounded plan over captured native identities. No text, native handles,
// translated labels or last-message heuristic decides an element category.
#include "native_ui.h"
#include "ui_element_calibration.h"
namespace gx_ui_element {
enum {MaxCards=64,PhoneFeedFirst=0x1100,PhoneFeedEnd=0x1200,NotificationFeedEnd=0x1100};
inline unsigned kind(const GxUiItem&r){
    if(r.id==3)return Money;
    if(r.id==10||r.id==11||r.id==12)return Help;
    if(r.id==15)return Subtitles;
    if(r.id==6||r.id==7||r.id==9)return Location;
    if(r.id==2||r.id==4||r.id==13||r.id==17)return Status;
    if(r.id>=PhoneFeedFirst&&r.id<PhoneFeedEnd)return Phone;
    if(r.id>=GX_UI_FEED&&r.id<NotificationFeedEnd)return Notifications;
    return Other;
}
struct Group {unsigned x,y,width,height,count;Group():x(0),y(0),width(0),height(0),count(0){} };
struct Plan {
    unsigned count,legacyWidth,legacyHeight,customWidth,customHeight,admittedMask,rejectedMask;
    bool legacy,moneyOnly;
    gx_native_ui::Placement original[MaxCards],custom[MaxCards];
    unsigned kinds[MaxCards];Group groups[Count];
    Plan():count(0),legacyWidth(0),legacyHeight(0),customWidth(0),customHeight(0),admittedMask(0),rejectedMask(0),legacy(false),moneyOnly(false){}
};
// Caller supplies native priority-sorted, RIGHT-only cards. Reserve the complete
// legacy rectangle, with holes only AFTER custom admission. This keeps automatic
// cards' pixel positions/density unchanged when another card is configured.
inline Plan plan(const GxUiItem*items,unsigned count,const GxUiElementProfiles&profiles,unsigned availableLayers){
    Plan p;if(!items||!valid(profiles)||count>MaxCards)return p;
    gx_native_ui::Layout original;unsigned present=0,individual=0;
    for(unsigned i=0;i<count;++i){
        if(gx_native_ui::fitRoute(items[i])!=GX_UI_RIGHT_WRIST)continue;
        const gx_native_ui::Placement dst=original.append(items[i]);if(!dst.placed)continue;
        const unsigned index=p.count++;p.original[index]=dst;p.kinds[index]=kind(items[i]);
        present|=1u<<p.kinds[index];
    }
    // The native producer already rejected capacity; a sparse input is not
    // silently relabelled/reindexed. Fail to automatic at the caller instead.
    if(p.count!=count){p=Plan();return p;}
    p.legacyWidth=original.width();p.legacyHeight=original.height();
    p.moneyOnly=count==1&&items[0].id==3;if(!count)return p;
    for(unsigned k=0;k<Count;++k)if((present&(1u<<k))&&profiles.element[k].individual)individual|=1u<<k;
    unsigned requested=0;for(unsigned k=0;k<Count;++k)if(individual&(1u<<k))++requested;
    if(availableLayers&&individual==present&&requested<=availableLayers)p.admittedMask=individual;
    else if(availableLayers){unsigned left=availableLayers-1; // mandatory automatic fallback slot
        // Stable priority: money/help/phone then notifications; no content-dependent identity.
        const unsigned order[RightCount]={Money,Help,Phone,Notifications,Subtitles,Location,Status,Other};
        for(unsigned i=0;i<RightCount&&left;++i)if(individual&(1u<<order[i])){p.admittedMask|=1u<<order[i];--left;}
    }
    p.rejectedMask=individual&~p.admittedMask;p.legacy=(present&~p.admittedMask)!=0;
    gx_native_ui::Layout custom;
    for(unsigned k=0;k<Count;++k)if(p.admittedMask&(1u<<k)){
        Group&g=p.groups[k];
        for(unsigned i=0;i<count;++i)if(p.kinds[i]==k){
            const gx_native_ui::Placement dst=custom.append(items[i]);
            // Same finite cards, no duplicated padding/copies: this subset fits
            // whenever the original layout fitted. Explicitly reject otherwise.
            if(!dst.placed){p.admittedMask=0;p.rejectedMask=individual;p.legacy=true;
                p.customWidth=p.customHeight=0;return p;}
            p.custom[i]=dst;if(!g.count){g.x=0;g.y=dst.y-16;}
            if(g.width<dst.width+32)g.width=dst.width+32;
            g.height=dst.y+dst.height+16-g.y;++g.count;
        }
    }
    p.customWidth=custom.width();p.customHeight=custom.height();return p;
}
}
