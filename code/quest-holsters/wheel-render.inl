constexpr int atlasWidth=1200,wheelHeight=852,atlasHeight=wheelHeight+pixels;
constexpr float metresPerPixel=.00057f;
struct AmmoCount {int clip=-1,total=-1;};
std::array<AmmoCount,weaponCount> cardAmmo;
std::wstring ammoText(AmmoCount count){
 if(count.total<0)return {};
 if(!count.total)return L"0/0";
 if(count.clip<0||count.clip>count.total)return std::to_wstring(count.total);
 return std::to_wstring(count.clip)+L"/"+std::to_wstring(count.total-count.clip);
}
struct WheelState {
 std::array<int,9> items{};
 std::array<int,8> count{},ordinal{},clip{},total{};
 int hover=-1;
 bool operator==(const WheelState& b) const{
    return items==b.items&&count==b.count&&ordinal==b.ordinal&&clip==b.clip&&total==b.total&&hover==b.hover;
 }
};
WheelState uploadedWheel;
WheelState wheelState(ULONGLONG now){
 WheelState w;w.items=pageCards();
 if(editor){
    w.hover=dragSlot;
    if(w.hover<0)for(auto& h:hands)if(h.seen&&now-h.seen<175&&h.zone>=0)w.hover=h.zone;
    return w;
 }
 for(int i=0;i<weaponCount;i++)if(available[i]){
    int k=catalog[i].category;++w.count[k];if(w.items[k]==i){
       w.ordinal[k]=w.count[k];w.clip[k]=cardAmmo[i].clip;w.total[k]=cardAmmo[i].total;}
 }
 for(int i=0;i<2;i++)if((editor?(editorHand<0||editorHand==i):menuHand==i)&&now-hands[i].seen<175){
    int hit=touchedCard(hands[i].position);if(hit>=0)w.hover=hit;
 }
 return w;
}
#include "hud-art.inl"
bool paintWheel(std::vector<uint32_t>& data,const WheelState& w){
 BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=atlasWidth;bi.bmiHeader.biHeight=-atlasHeight;
 bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
 HDC dc=CreateCompatibleDC(nullptr);void* bits=nullptr;
 HBITMAP bitmap=dc?CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0):nullptr;
 if(!bitmap){if(dc)DeleteDC(dc);return false;}
 auto old=SelectObject(dc,bitmap);PatBlt(dc,0,0,atlasWidth,atlasHeight,BLACKNESS);
 auto font=[](int n,int weight){return CreateFontW(-n,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");};
 auto detailFont=font(20,FW_SEMIBOLD),titleFont=font(23,FW_SEMIBOLD);
 auto oldFont=SelectObject(dc,detailFont);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
 std::fill(data.begin(),data.end(),0u);
 bool editing=w.items[8]==doneCard;
 if(!editing)wheelBackdrop(data,w);
 auto label=[&](const std::wstring& text,int x,int y,int width,HFONT face){
    SelectObject(dc,face);RECT rect{x-width/2,y-15,x+width/2,y+15};
    DrawTextW(dc,text.c_str(),-1,&rect,DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
 };
 int cx=atlasWidth/2,cy=wheelHeight/2;
 if(editing){
    // The extra atlas row is sampled by four independently positioned stereo quads.
    for(int y=0;y<pixels;y++)for(int x=0;x<pixels;x++){
       float dx=(2.f*x+1)/pixels-1,dy=(2.f*y+1)/pixels-1;
       float radius=sqrtf(dx*dx+dy*dy);
       unsigned alpha=unsigned(255*std::clamp((.94f-radius)*pixels,0.f,1.f));
       unsigned shade=unsigned(115*std::clamp(1-radius*.6f-(dx+dy)*.25f,0.f,1.f));
       data[(wheelHeight+y)*atlasWidth+x]=(alpha<<24)|(255u<<16)|((125u+shade)<<8)|(40u+shade);
    }
    for(int y=cy-39;y<cy+39;y++)for(int x=cx-268;x<cx+268;x++)data[y*atlasWidth+x]=0xb0252423u;
    const wchar_t* names[]={L"Left hip",L"Right hip",L"Left shoulder",L"Right shoulder"};
    label(w.hover>=0&&w.hover<4?names[w.hover]:L"Move holsters",cx,cy-15,450,titleFont);
    label(L"Hold right stick down to save and exit",cx,cy+15,500,detailFont);
 }else{
    for(int i=0;i<8;i++){
       int item=w.items[i];if(item<0)continue;
       int x=int(cx+cards[i].x/metresPerPixel),y=int(cy-(cards[i].y-wheelY)/metresPerPixel);
       const auto& icon=hudIcons()[item];
       if(icon.rgba){
          for(int iy=0;iy<icon.height;iy++)for(int ix=0;ix<icon.width;ix++){
             auto rgba=icon.rgba+4*(iy*icon.width+ix);
             auto& dst=data[(y-icon.height/2+iy-7)*atlasWidth+x-icon.width/2+ix];
             hudOver(dst,rgba[0],rgba[1],rgba[2],rgba[3]);
          }
       }else label(catalog[item].name,x,y-7,190,detailFont);
       if(i!=4)label(ammoText({w.clip[i],w.total[i]}),x,y+44,180,detailFont);
    }
    if(w.hover>=0&&w.hover<8&&w.items[w.hover]>=0){
       int k=w.hover;label(catalog[w.items[k]].name,cx,cy-10,340,titleFont);
       if(w.count[k]>1)label(std::to_wstring(w.ordinal[k])+L"/"+std::to_wstring(w.count[k]),cx,cy+19,210,detailFont);
    }
 }
 GdiFlush();auto src=static_cast<uint32_t*>(bits);
 for(int y=0;y<wheelHeight-2;y++)for(int x=0;x<atlasWidth-2;x++){
    unsigned a=src[y*atlasWidth+x]&255u;if(a)hudOver(data[(y+2)*atlasWidth+x+2],0,0,0,a);
 }
 for(int i=0;i<atlasWidth*wheelHeight;i++){auto a=src[i]&255u;if(a)hudOver(data[i],255,255,255,a);}
 SelectObject(dc,oldFont);SelectObject(dc,old);
 DeleteObject(detailFont);DeleteObject(titleFont);DeleteObject(bitmap);DeleteDC(dc);return true;
}
