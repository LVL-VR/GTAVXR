#pragma once
#include <stdint.h>
namespace gx_menyoo_route {
enum { Route=7,Id=0x6d6e7501 };
template<class Item>bool belongs(const Item&item){return item.route==Route&&item.id==Id;}
inline bool physicalSize(unsigned width,unsigned height,float&metresWide,float&metresHigh){
 if(!width||!height||width>3840||height>2160)return false;
 const float ratio=static_cast<float>(height)/width;float wide=.7f,high=wide*ratio;
 if(high>1.25f){high=1.25f;wide=high/ratio;}
 if(!(wide>0&&high>0))return false;metresWide=wide;metresHigh=high;return true;
}
}
