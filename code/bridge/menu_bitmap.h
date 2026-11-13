#pragma once
#include "overlay.h"
#include <windows.h>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <cmath>
namespace gx_ui {
enum {Width=896,Height=960};
inline bool valid(const GxOverlay&s){
    if(s.size!=sizeof(s)||(s.version!=1&&s.version!=2&&s.version!=3)||s.visible>1||s.row_count>12||
       (s.version==2&&(s.row_count<1||s.row_count>8))||
       (s.row_count&&(s.version==2?s.selected>s.row_count:s.selected>=s.row_count))||!memchr(s.title,0,sizeof(s.title))||!memchr(s.footer,0,sizeof(s.footer)))return false;
    for(unsigned r=0;r<s.row_count;++r)if(!memchr(s.rows[r],0,sizeof(s.rows[r])))return false;return true;
}
// GDI renders into an OWNED memory DIB, never a window or desktop DC.
class Bitmap {
    HDC dc{};HBITMAP dib{};HGDIOBJ old{};HFONT title{},body{},foot{};void*bits{};
    void rectangle(int x,int y,int w,int h,COLORREF color){HBRUSH brush=CreateSolidBrush(color);if(!brush)throw std::runtime_error("UI_BRUSH");RECT r{x,y,x+w,y+h};FillRect(dc,&r,brush);DeleteObject(brush);}
    void text(int x,int y,int w,int h,const char*s,HFONT font,COLORREF color){auto previous=SelectObject(dc,font);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);RECT r{x,y,x+w,y+h};DrawTextA(dc,s,-1,&r,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);SelectObject(dc,previous);}
public:
    Bitmap(){
        dc=CreateCompatibleDC(nullptr);if(!dc)throw std::runtime_error("UI_MEMORY_DC");
        BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=Width;bi.bmiHeader.biHeight=-Height;
        bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
        dib=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
        if(!dib){DeleteDC(dc);dc=nullptr;throw std::runtime_error("UI_DIB");}old=SelectObject(dc,dib);
        title=CreateFontW(-44,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");
        body=CreateFontW(-30,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");
        foot=CreateFontW(-23,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");
        if(!title||!body||!foot){clear();throw std::runtime_error("UI_FONT");}
    }
    Bitmap(const Bitmap&)=delete;Bitmap&operator=(const Bitmap&)=delete;
    void clear(){if(dc&&old)SelectObject(dc,old);if(title)DeleteObject(title);if(body)DeleteObject(body);if(foot)DeleteObject(foot);if(dib)DeleteObject(dib);if(dc)DeleteDC(dc);dc=nullptr;dib=nullptr;title=body=foot=nullptr;}
    ~Bitmap(){clear();}
    std::vector<unsigned char> paint(const GxOverlay&s){
        if(!valid(s))throw std::runtime_error("UI_STATE");
        memset(bits,0,Width*Height*4);rectangle(16,16,Width-32,Height-32,RGB(18,24,23));
        rectangle(16,16,Width-32,8,RGB(106,207,130));
        text(44,36,Width-88,70,s.title,title,RGB(242,245,242));
        text(44,104,Width-88,32,"STORY MODE  /  EL JUEGO CONTINUA",foot,RGB(141,195,152));
        for(unsigned r=0;r<s.row_count;++r){
            if(s.version==2){
                const double a=6.283185307179586*r/s.row_count;
                const int x=Width/2+int(260*sin(a))-138,y=478-int(280*cos(a))-36;
                rectangle(x,y,276,72,r==s.selected?RGB(52,102,65):RGB(29,43,35));
                text(x+8,y,260,72,s.rows[r],body,RGB(241,244,241));continue;
            }
            const int y=158+int(r)*58;
            if(r==s.selected)rectangle(34,y,Width-68,52,RGB(52,102,65));
            text(54,y,Width-108,52,s.rows[r],body,RGB(241,244,241));
        }
        rectangle(34,864,Width-68,2,RGB(67,80,73));
        text(44,878,Width-88,31,s.version==2?"TU stick: arma    Suelta grip: equipar    Clic: pagina":"Stick: navegar    A: elegir    B / Menu: volver",foot,RGB(207,214,209));
        text(44,915,Width-88,27,s.footer,foot,RGB(146,186,157));
        GdiFlush();std::vector<unsigned char> out(Width*Height*4);memcpy(out.data(),bits,out.size());
        for(size_t n=0;n<out.size();n+=4){const bool ink=out[n]||out[n+1]||out[n+2];
            const bool background=out[n]==23&&out[n+1]==24&&out[n+2]==18;
            out[n+3]=ink?(background?208:255):0;
        }
        return out; // straight-alpha BGRA; XR layer declares UNPREMULTIPLIED_ALPHA.
    }
};
}
