#pragma once
#include "appearance.h"
#include "overlay.h"
#include <windows.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>
#include <cmath>
namespace gx_menyoo_ui {
enum {Width=896,Height=960};
inline bool valid(const GxOverlay&s){
 if(s.size!=sizeof(s)||s.version!=1||s.visible>1||s.row_count>12||(s.row_count&&s.selected>=s.row_count)||
    !memchr(s.title,0,sizeof(s.title))||!memchr(s.footer,0,sizeof(s.footer)))return false;
 for(unsigned i=0;i<s.row_count;++i)if(!memchr(s.rows[i],0,sizeof(s.rows[i])))return false;return true;
}
// Own-memory GDI glyph mask + straight-alpha composition. No desktop/window DC,
// texture download, game font ownership, native Draw calls or Scaleform advance.
// Existing version2 radial presentation is deliberately left to gx_ui::Bitmap.
class Bitmap {
 HDC dc_;HBITMAP dib_;HGDIOBJ previous_;HFONT title_,body_,foot_;void*bits_;
 std::vector<unsigned char> pixels_;
 void cleanup(){if(dc_&&previous_)SelectObject(dc_,previous_);if(title_)DeleteObject(title_);if(body_)DeleteObject(body_);if(foot_)DeleteObject(foot_);if(dib_)DeleteObject(dib_);if(dc_)DeleteDC(dc_);dc_=NULL;dib_=NULL;title_=body_=foot_=NULL;previous_=NULL;bits_=NULL;}
 void pixel(unsigned char*destination,const menyoo_vr::Colour&c,unsigned coverage){
  const unsigned sourceAlpha=(c.a*coverage+127)/255,destAlpha=destination[3];
  const unsigned alphaScaled=sourceAlpha*255+destAlpha*(255-sourceAlpha);
  const unsigned outAlpha=(alphaScaled+127)/255;
  if(!outAlpha){memset(destination,0,4);return;}
  const unsigned channels[3]={c.b,c.g,c.r};
  for(unsigned i=0;i<3;++i){const unsigned numerator=channels[i]*sourceAlpha*255+destination[i]*destAlpha*(255-sourceAlpha);
   destination[i]=static_cast<unsigned char>((numerator+alphaScaled/2)/alphaScaled);}
  destination[3]=static_cast<unsigned char>(outAlpha);
 }
 void rectangle(int x,int y,int w,int h,const menyoo_vr::Colour&colour){
  for(int row=y;row<y+h;++row)for(int column=x;column<x+w;++column)
   if(row>=0&&row<Height&&column>=0&&column<Width)pixel(&pixels_[(row*Width+column)*4],colour,255);
 }
 void text(int x,int y,int w,int h,const char*utf8,HFONT font,const menyoo_vr::Colour&colour,bool centered=false,bool multiline=false,bool outline=false){
  const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8,-1,NULL,0);
  if(!count)throw std::runtime_error("MENU_UTF8");std::vector<wchar_t>wide(count);
  if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8,-1,&wide[0],count))throw std::runtime_error("MENU_UTF8");
  memset(bits_,0,Width*Height*4);HGDIOBJ old=SelectObject(dc_,font);SetTextColor(dc_,RGB(255,255,255));SetBkMode(dc_,TRANSPARENT);
  RECT bounds={x,y,x+w,y+h};unsigned flags=DT_NOPREFIX|(centered?DT_CENTER:DT_LEFT)|(multiline?DT_WORDBREAK:DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
  DrawTextW(dc_,&wide[0],-1,&bounds,flags);SelectObject(dc_,old);GdiFlush();
  const unsigned char*mask=static_cast<const unsigned char*>(bits_);
  if(outline&&colour.a){const menyoo_vr::Colour stroke={0,0,0,colour.a};
   for(int row=1;row<Height-1;++row)for(int column=1;column<Width-1;++column){unsigned coverage=0;
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){const size_t k=((row+dy)*Width+column+dx)*4;
     const unsigned value=(unsigned(mask[k])+mask[k+1]+mask[k+2])/3;if(value>coverage)coverage=value;}
    if(coverage)pixel(&pixels_[(row*Width+column)*4],stroke,coverage);}}
  // GDI can draw glyph overhangs outside the supplied horizontal bounds.
  // Whole-DIB mask traversal preserves them without unsafe clipped indexing.
  for(size_t i=0;i<pixels_.size();i+=4){const unsigned coverage=(unsigned(mask[i])+mask[i+1]+mask[i+2])/3;if(coverage)pixel(&pixels_[i],colour,coverage);}
 }
public:
 Bitmap():dc_(NULL),dib_(NULL),previous_(NULL),title_(NULL),body_(NULL),foot_(NULL),bits_(NULL),pixels_(Width*Height*4){
  dc_=CreateCompatibleDC(NULL);if(!dc_)throw std::runtime_error("MENU_MEMORY_DC");
  BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=Width;info.bmiHeader.biHeight=-Height;
  info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
  dib_=CreateDIBSection(dc_,&info,DIB_RGB_COLORS,&bits_,NULL,0);
  if(!dib_){cleanup();throw std::runtime_error("MENU_MEMORY_DIB");}previous_=SelectObject(dc_,dib_);
  title_=CreateFontW(-48,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");
  body_=CreateFontW(-30,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");
  foot_=CreateFontW(-23,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");
  if(!title_||!body_||!foot_){cleanup();throw std::runtime_error("MENU_FONT");}
 }
 ~Bitmap(){cleanup();}
private:
 Bitmap(const Bitmap&);Bitmap&operator=(const Bitmap&);
public:
 const std::vector<unsigned char>&paint(const GxOverlay&s,const menyoo_vr::Appearance&appearance){
  if(!valid(s)||!menyoo_vr::validAppearance(appearance))throw std::runtime_error("MENU_APPEARANCE_ABI");
  memset(&pixels_[0],0,pixels_.size());const menyoo_vr::Colour*colours=appearance.colour;
  const int controlsY=146+int(s.row_count)*55+12,helpY=controlsY+50;
  rectangle(16,16,Width-32,helpY+65,colours[menyoo_vr::Background]);
  rectangle(16,16,Width-32,118,colours[menyoo_vr::Header]);
  const menyoo_vr::Colour&titleColour=colours[menyoo_vr::TitleText];
  // Match upstream AddTitle's white-text outline for readable cyan headers.
  text(40,30,Width-80,90,s.title,title_,titleColour,true,false,titleColour.r>240&&titleColour.g>240&&titleColour.b>240);
  for(unsigned i=0;i<s.row_count;++i){const int y=146+int(i)*55;
   if(i==s.selected)rectangle(16,y,Width-32,55,colours[menyoo_vr::Selection]);
   text(42,y,Width-84,55,s.rows[i],body_,colours[i==s.selected?menyoo_vr::SelectedText:menyoo_vr::OptionText]);
  }
  const bool spanish=appearance.language==menyoo_vr::Spanish;
  text(42,controlsY,Width-84,38,spanish?"Stick: navegar  /  A: elegir  /  B: volver":"Stick: navigate  /  A: select  /  B: back",foot_,colours[menyoo_vr::HelpText]);
  text(42,helpY,Width-84,65,s.footer,foot_,colours[menyoo_vr::HelpText],false,true);
  return pixels_; // straight-alpha; retain XR UNPREMULTIPLIED_ALPHA contract.
 }
};
}
