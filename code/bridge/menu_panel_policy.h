#pragma once
#include "native_ui.h"
namespace gx_menu_panel {
struct Region {double l,t,r,b;bool any;
    Region():l(0),t(0),r(0),b(0),any(false){}
    bool add(const GxUiItem&i,unsigned w,unsigned h){
        if(i.route!=GX_UI_MENU_PANEL||!gx_native_ui::bounded(i,w,h))return false;
        const double x=double(i.x)/w,y=double(i.y)/h,rr=double(i.x+i.width)/w,bb=double(i.y+i.height)/h;
        if(!any){l=x;t=y;r=rr;b=bb;any=true;}else{if(x<l)l=x;if(y<t)t=y;if(rr>r)r=rr;if(bb>b)b=bb;}return true;
    }
};
struct Quad {double x,y,width,height;Quad():x(0),y(0),width(0),height(0){} };
inline bool place(const Region&bounds,const GxUiItem&i,unsigned w,unsigned h,Quad&out,int percent=100){
    if(!bounds.any||bounds.r<=bounds.l||bounds.b<=bounds.t||!w||!h||percent<40||percent>200||!gx_native_ui::bounded(i,w,h))return false;
    const double aspect=double(h)/w;
    const double physicalW=bounds.r-bounds.l,physicalH=(bounds.b-bounds.t)*aspect;
    const double sx=.82/physicalW,sy=.86/physicalH,scale=(sx<sy?sx:sy)*percent*.01;
    Quad q;q.width=scale*i.width/w;q.height=scale*aspect*i.height/h;
    q.x=scale*((i.x+i.width*.5)/w-(bounds.l+bounds.r)*.5);
    q.y=scale*aspect*((bounds.t+bounds.b)*.5-(i.y+i.height*.5)/h);
    if(!_finite(q.x)||!_finite(q.y)||!_finite(q.width)||!_finite(q.height)||q.width<=0||q.height<=0)return false;
    out=q;return true;
}
}
