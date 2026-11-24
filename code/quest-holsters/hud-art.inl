struct HudIcon {int width=0,height=0;const unsigned char* rgba=nullptr;};
const std::array<HudIcon,weaponCount>& hudIcons(){
 static const auto icons=[]{
    std::array<HudIcon,weaponCount> result{};
    HMODULE module=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&hudIcons),&module);
    auto res=FindResourceW(module,MAKEINTRESOURCEW(101),MAKEINTRESOURCEW(10));
    if(!res)return result;
    auto size=SizeofResource(module,res);auto p=static_cast<const unsigned char*>(LockResource(LoadResource(module,res)));
    if(!p||size<8||memcmp(p,"GWH1",4))return result;
    unsigned count;memcpy(&count,p+4,4);auto end=p+size;p+=8;
    for(unsigned n=0;n<count&&end-p>=8;n++){
       unsigned hash;unsigned short w,h;memcpy(&hash,p,4);memcpy(&w,p+4,2);memcpy(&h,p+6,2);p+=8;
       if(w>170||h>78||end-p<4*w*h)break;
       for(int i=0;i<weaponCount;i++)if(catalog[i].hash==hash){result[i]={w,h,p};break;}
       p+=4*w*h;
    }
    return result;
 }();
 return icons;
}
// The atlas uses straight RGBA for XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT.
void hudOver(uint32_t& bg,unsigned red,unsigned green,unsigned blue,unsigned alpha){
 if(!alpha)return;
 unsigned back=((bg>>24)*(255-alpha)+127)/255,out=alpha+back;
 auto ch=[&](unsigned v,int shift){return ((v*alpha+((bg>>shift)&255)*back+out/2)/out)<<shift;};
 bg=(out<<24)|ch(red,0)|ch(green,8)|ch(blue,16);
}
void wheelBackdrop(std::vector<uint32_t>& data,const WheelState& w){
 // Cache the geometric sector map once. Subsequent hover changes only recolor pixels.
 static const auto map=[]{
    std::vector<unsigned char> cells(atlasWidth*wheelHeight,255);
    for(int y=0;y<wheelHeight;y++)for(int x=0;x<atlasWidth;x++){
       float dx=(x+.5f-atlasWidth*.5f)*metresPerPixel,dy=(wheelHeight*.5f-y-.5f)*metresPerPixel;
       float radius=sqrtf(dx*dx+dy*dy);if(radius<innerRadius||radius>outerRadius)continue;
       float angle=atan2f(dx,dy),sectorAngle=angle*1.27323954f;
       float gap=(.5f-fabsf(sectorAngle-roundf(sectorAngle)))*.78539816f*radius;
       if(gap<metresPerPixel)continue;
       cells[y*atlasWidth+x]=angleSector(dx,dy)+(radius>outerRadius-.006f?8:0);
    }
    return cells;
 }();
 for(size_t i=0;i<map.size();i++){
    unsigned cell=map[i];if(cell==255)continue;
    int sector=cell&7;bool focus=w.hover==sector;
    data[i]=focus?(cell>=8?0xeccca56bu:0x95544435u):(w.items[sector]>=0?0x90161514u:0x55161514u);
 }
}
