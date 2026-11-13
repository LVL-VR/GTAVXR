#pragma once
// A bounded set of matched pairs, not per-frame readback. Native final pre-HUD BGRA8 only.
namespace native_xr {
inline void save_first_image(Session&x,ID3D11Texture2D*source,unsigned eye,const std::wstring&directory){
    D3D11_TEXTURE2D_DESC d{};source->GetDesc(&d);demand(x.compatible(d),"SCREENSHOT_SHAPE");
    d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.MiscFlags=0;
    ComPtr<ID3D11Texture2D> staging;demand(SUCCEEDED(x.device->CreateTexture2D(&d,nullptr,&staging)),"SCREENSHOT_STAGING");
    x.context->CopyResource(staging.Get(),source);D3D11_MAPPED_SUBRESOURCE m{};
    demand(SUCCEEDED(x.context->Map(staging.Get(),0,D3D11_MAP_READ,0,&m)),"SCREENSHOT_MAP");
    HANDLE f=INVALID_HANDLE_VALUE;
    try {
        const auto prefix=x.submitted==0?std::wstring(L"/first"):L"/sample-"+std::to_wstring(x.frame);
        const auto name=directory+prefix+(eye?L"-R.bmp":L"-L.bmp");
        f=CreateFileW(name.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        demand(f!=INVALID_HANDLE_VALUE,"SCREENSHOT_CREATE");
        auto write=[&](const void*p,DWORD n){DWORD got=0;demand(WriteFile(f,p,n,&got,nullptr)&&got==n,"SCREENSHOT_WRITE");};
        BITMAPFILEHEADER fh{};BITMAPINFOHEADER ih{};fh.bfType=0x4d42;fh.bfOffBits=sizeof(fh)+sizeof(ih);fh.bfSize=fh.bfOffBits+d.Width*d.Height*4;
        ih.biSize=sizeof(ih);ih.biWidth=d.Width;ih.biHeight=-LONG(d.Height);ih.biPlanes=1;ih.biBitCount=32;ih.biCompression=BI_RGB;
        write(&fh,sizeof(fh));write(&ih,sizeof(ih));for(UINT y=0;y<d.Height;++y)write(static_cast<unsigned char*>(m.pData)+y*m.RowPitch,d.Width*4);
        demand(FlushFileBuffers(f),"SCREENSHOT_FLUSH");CloseHandle(f);f=INVALID_HANDLE_VALUE;
    }catch(...){if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);x.context->Unmap(staging.Get(),0);throw;}
    x.context->Unmap(staging.Get(),0);
    x.log(std::string(x.submitted==0?"FIRST_IMAGE eye=":"SAMPLED_IMAGE eye=")+std::to_string(eye)+" frame="+std::to_string(x.frame)+" generation="+std::to_string(x.generation));
}
}
