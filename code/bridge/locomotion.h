#pragma once
// VS2012-compatible policy shared by native integration and offline controls.
#include "input.h"
#include <math.h>
#include <float.h>
#include <string.h>
namespace gx_loco {
inline bool finite(double x){return _finite(x)!=0;}
inline double clamp(double x,double lo,double hi){return x<lo?lo:(x>hi?hi:x);}
inline double wrap(double x){const double pi=3.14159265358979323846;return x-2*pi*floor((x+pi)/(2*pi));}
inline double yaw(const GxPose&p){return atan2(2*(p.qw*p.qy+p.qx*p.qz),1-2*(p.qy*p.qy+p.qx*p.qx));}
inline double native_yaw(const double*camera){return atan2(camera[8],-camera[9]);}
inline void apply_yaw_reference(GxPose&reference,const double*camera,double worldOffset){
    const double a=wrap(native_yaw(camera)-worldOffset);
    reference.qx=reference.qz=0;reference.qy=sin(a/2);reference.qw=cos(a/2);
}
struct Pad {
    int lx,ly,rx,ry,lt,rt,lb,rb,a,b,x,y,ls,rs,start,select,up,down,left,right;
    bool recenter;double snap,heightStep;
    Pad(){memset(this,0,sizeof(*this));}
};
class Mapping {
    bool menuHeld,longMenu,turnLatch,shiftLatch,leftGrip,rightGrip,heightLatch,heightGesture;
    double menuStart;
    static bool schmitt(float f,bool old){return f>=(old?.45f:.65f);}
    static int axis(float f){return (int)(clamp(f,-1,1)*127.0);}
public:
    Mapping(){reset();}
    void reset(){menuHeld=longMenu=turnLatch=shiftLatch=leftGrip=rightGrip=heightGesture=false;heightLatch=true;menuStart=0;}
    Pad map(const GxInput&i,double seconds,bool paused,bool seated){
        Pad p;
        if(i.version!=1||i.size!=sizeof(i)||!i.active_hands||!finite(seconds)){reset();return p;}
        for(int h=0;h<2;++h)if(!finite(i.stick[h][0])||!finite(i.stick[h][1])||!finite(i.trigger[h])||!finite(i.grip[h])){reset();return p;}
        const unsigned mask=((i.active_hands&1)?(GX_X|GX_Y|GX_LCLICK|GX_MENU):0)|((i.active_hands&2)?(GX_A|GX_B|GX_RCLICK):0);
        const unsigned b=i.buttons&mask;
        p.a=(b&GX_A)?255:0;p.b=(b&GX_B)?255:0;p.x=(b&GX_X)?255:0;p.y=(b&GX_Y)?255:0;
        p.ls=(b&GX_LCLICK)?255:0;p.rs=(b&GX_RCLICK)?255:0;
        if(i.active_hands&1){p.lx=axis(i.stick[0][0]);p.ly=-axis(i.stick[0][1]);p.lt=(int)(clamp(i.trigger[0],0,1)*255);leftGrip=schmitt(i.grip[0],leftGrip);}else leftGrip=false;
        if(i.active_hands&2){p.rx=axis(i.stick[1][0]);p.ry=-axis(i.stick[1][1]);p.rt=(int)(clamp(i.trigger[1],0,1)*255);rightGrip=schmitt(i.grip[1],rightGrip);}else rightGrip=false;
        p.lb=leftGrip?255:0;p.rb=rightGrip?255:0;
        // Both stick clicks form a deliberate D-pad modifier. Press both at neutral first;
        // consume click actions while shifted so crouch/look-back cannot fire simultaneously.
        if(p.ls&&p.rs)shiftLatch=true;
        if(!p.ls&&!p.rs)shiftLatch=false;
        if(shiftLatch){p.ls=p.rs=0;p.up=p.ry<-64?255:0;p.down=p.ry>64?255:0;p.left=p.rx<-64?255:0;p.right=p.rx>64?255:0;p.rx=p.ry=0;}
        // Short Menu = Start; long Menu = recenter only. No pause on initial press.
        const bool m=(b&GX_MENU)!=0;
        if(m&&!menuHeld){menuStart=seconds;longMenu=heightGesture=false;heightLatch=true;}
        if(m&&(b&GX_X)){if(!longMenu)p.select=255;longMenu=true;p.x=0;}
        // Calibration is intentional Menu + neutral-then-vertical stick, never a held turn
        // on Menu entry. Once used, this Menu gesture cannot also pause/recenter/select.
        if(m&&!(b&GX_X)&&!shiftLatch&&(i.active_hands&2)){
            if(abs(p.ry)<45)heightLatch=false;
            if(abs(p.ry)>95&&!heightLatch){p.heightStep=p.ry<0?.02:-.02;heightLatch=true;heightGesture=longMenu=true;}
        }
        if(m&&!longMenu&&seconds-menuStart>=.8){p.recenter=true;longMenu=true;}
        if(!m&&menuHeld&&!longMenu)p.start=255;
        menuHeld=m;
        // Snap only on foot; native right stick retained for vehicles, weapon-wheel and menus.
        if(m&&heightGesture){
            const double step=p.heightStep;p=Pad();p.heightStep=step;turnLatch=true;
        }else if(!paused&&!seated&&!p.lb&&!shiftLatch){
            if(abs(p.rx)<45)turnLatch=false;
            if(abs(p.rx)>95&&!turnLatch){p.snap=(p.rx>0?-1:1)*3.14159265358979323846/6;turnLatch=true;}
            p.rx=p.ry=0;
        }else turnLatch=true; // neutral required after a context transition.
        return p;
    }
};
class HeightCalibration {
    double trim[2],standingY;bool standingReady;
public:
    HeightCalibration():standingY(0),standingReady(false){trim[0]=trim[1]=0;}
    void newSpace(){standingReady=false;}
    void adjust(bool seated,double step){if(finite(step))trim[seated?1:0]=clamp(trim[seated?1:0]+step,-.75,.75);}
    double value(bool seated)const{return trim[seated?1:0];}
    void recenter(bool seated,double headY){
        if(!finite(headY))return;
        trim[seated?1:0]=0;
        if(!seated){standingY=headY;standingReady=true;}
    }
    void apply(GxPose&reference,bool seated)const{
        if(!seated&&standingReady)reference.y=standingY;
        reference.y-=trim[seated?1:0]; // subtracting reference raises the shared world camera.
    }
};
struct Vec2 {double x,y;Vec2(double a=0,double b=0):x(a),y(b){} };
// Convert horizontal tracking once to collision-mediated velocity. Requested motion is NOT
// accepted motion. Compensation prevents double camera translation, not native collision.
class RoomMotion {
    Vec2 consumed;bool ready;uint64_t used;
public:
    RoomMotion():ready(false),used(0){}
    void reset(){ready=false;used=0;}
    void anchor(Vec2 physical){consumed=physical;ready=true;used=0;}
    bool isReady()const{return ready;}
    Vec2 origin()const{return consumed;}
    Vec2 residual(Vec2 physical)const{return Vec2(physical.x-consumed.x,physical.y-consumed.y);}
    Vec2 velocity(Vec2 physical,uint64_t sequence,double dt,bool eligible,bool&discontinuity){
        discontinuity=false;
        if(!finite(physical.x)||!finite(physical.y)){reset();return Vec2();}
        if(!finite(dt)||dt<=0||dt>.2||!sequence){anchor(physical);used=sequence;return Vec2();}
        if(!ready||!eligible){anchor(physical);used=sequence;return Vec2();}
        if(sequence<=used)return Vec2();
        Vec2 d=residual(physical);consumed=physical;used=sequence;
        // No position write/teleport. A tracking discontinuity rebases rather than launches.
        if(d.x*d.x+d.y*d.y>25*dt*dt){discontinuity=true;return Vec2();}
        return Vec2(d.x/dt,d.y/dt);
    }
     
    // consumes requested velocity over SIMULATION seconds. Do not scale tracking or
    // modify game timewarp. Explicit VR caps are NOT the engine's1000m/s assertion.
    Vec2 velocityTimed(Vec2 physical,uint64_t sequence,double simulationDt,double physicalDt,
                       bool eligible,bool&discontinuity){
        discontinuity=false;
        if(!finite(physical.x)||!finite(physical.y)){reset();return Vec2();}
        if(!finite(simulationDt)||simulationDt<.0001||simulationDt>.2||
           !finite(physicalDt)||physicalDt<=0||physicalDt>.2||!sequence){
            anchor(physical);used=sequence;discontinuity=true;return Vec2();
        }
        if(!ready||!eligible){anchor(physical);used=sequence;return Vec2();}
        if(sequence<=used)return Vec2();
        const Vec2 d=residual(physical);consumed=physical;used=sequence;
        const double d2=d.x*d.x+d.y*d.y;
        // At most25cm per consumed sample,5m/s physical and20m/s native request.
        // Gaps/extreme slowmo/tracking jumps rebase; no queued catch-up impulse.
        if(!finite(d2)||d2>.25*.25||d2>25*physicalDt*physicalDt||
           d2>400*simulationDt*simulationDt){discontinuity=true;return Vec2();}
        return Vec2(d.x/simulationDt,d.y/simulationDt);
    }
};
}
