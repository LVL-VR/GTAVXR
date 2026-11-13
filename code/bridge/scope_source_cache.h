#pragma once
// Optional owned world-before-UI copies. Does not retain original backbuffers,
// draw, alter game bindings, prove native phase identity or advance simulation.
#include <d3d11.h>
#include <stdint.h>
#include <string.h>
namespace gx_scope_source {
enum Stage { UnknownStage,WorldBeforeUi,CompositedUi };
class Cache {
 ID3D11Texture2D*texture_;ID3D11ShaderResourceView*eye_[2];D3D11_TEXTURE2D_DESC shape_;DXGI_FORMAT sampleFormat_;
 uint64_t frame_,generation_,latestFrame_;unsigned mask_;
 Cache(const Cache&);Cache&operator=(const Cache&);
 static bool format(DXGI_FORMAT v){return v==DXGI_FORMAT_R8G8B8A8_UNORM||v==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB||
  v==DXGI_FORMAT_B8G8R8A8_UNORM||v==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;}
 static DXGI_FORMAT family(DXGI_FORMAT v){
  if(v==DXGI_FORMAT_R8G8B8A8_TYPELESS||v==DXGI_FORMAT_R8G8B8A8_UNORM||v==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)return DXGI_FORMAT_R8G8B8A8_TYPELESS;
  if(v==DXGI_FORMAT_B8G8R8A8_TYPELESS||v==DXGI_FORMAT_B8G8R8A8_UNORM||v==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)return DXGI_FORMAT_B8G8R8A8_TYPELESS;
  return DXGI_FORMAT_UNKNOWN;
 }
 static bool shape(const D3D11_TEXTURE2D_DESC&v){return v.Width&&v.Height&&v.Width<=8192&&v.Height<=8192&&
  v.MipLevels==1&&v.ArraySize==1&&v.SampleDesc.Count==1&&!v.SampleDesc.Quality&&v.Usage==D3D11_USAGE_DEFAULT&&
  !v.CPUAccessFlags&&!(v.MiscFlags&(D3D11_RESOURCE_MISC_SHARED|D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX))&&family(v.Format)!=DXGI_FORMAT_UNKNOWN;}
 static bool same(const D3D11_TEXTURE2D_DESC&a,const D3D11_TEXTURE2D_DESC&b){return a.Width==b.Width&&a.Height==b.Height&&a.Format==b.Format;}
 bool allocate(ID3D11Device*device,const D3D11_TEXTURE2D_DESC&source,DXGI_FORMAT sampleFormat){
  D3D11_TEXTURE2D_DESC own=source;own.Format=family(source.Format);own.ArraySize=2;own.BindFlags=D3D11_BIND_SHADER_RESOURCE;own.MiscFlags=0;own.CPUAccessFlags=0;
  if(FAILED(device->CreateTexture2D(&own,NULL,&texture_)))return false;
  for(unsigned e=0;e<2;++e){D3D11_SHADER_RESOURCE_VIEW_DESC view={};view.Format=sampleFormat;
   view.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2DARRAY;view.Texture2DArray.MipLevels=1;view.Texture2DArray.FirstArraySlice=e;view.Texture2DArray.ArraySize=1;
   if(FAILED(device->CreateShaderResourceView(texture_,&view,&eye_[e]))){close();return false;}}
  shape_=source;sampleFormat_=sampleFormat;return true;
 }
public:
 Cache():texture_(NULL),sampleFormat_(DXGI_FORMAT_UNKNOWN),frame_(0),generation_(0),latestFrame_(0),mask_(0){eye_[0]=eye_[1]=NULL;memset(&shape_,0,sizeof(shape_));}
 ~Cache(){close();}
 void invalidate(){frame_=generation_=0;mask_=0;}
 void close(){invalidate();for(unsigned e=0;e<2;++e)if(eye_[e]){eye_[e]->Release();eye_[e]=NULL;}
  if(texture_){texture_->Release();texture_=NULL;}memset(&shape_,0,sizeof(shape_));sampleFormat_=DXGI_FORMAT_UNKNOWN;}
 // Caller holds the actual engine immediate-context ownership, and attests the
 // native producer phase. Stage alone is NOT a render-order/pixel proof.
 // Explicit view format describes the consumer's pixel interpretation (for XR,
 // the declared swapchain format). Never infer an encoding from TYPELESS.
 bool capture(ID3D11DeviceContext*context,ID3D11Texture2D*source,unsigned eye,uint64_t frame,uint64_t generation,Stage stage,DXGI_FORMAT sampleFormat=DXGI_FORMAT_UNKNOWN){
  if(!context||!source||eye>=2||!frame||!generation||stage!=WorldBeforeUi||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE){invalidate();return false;}
  if((eye==0&&latestFrame_&&frame<=latestFrame_)||(eye==1&&(frame_!=frame||generation_!=generation||mask_!=1))){invalidate();return false;}
  D3D11_TEXTURE2D_DESC description={};source->GetDesc(&description);if(!shape(description)||source==texture_){invalidate();return false;}
  if(sampleFormat==DXGI_FORMAT_UNKNOWN)sampleFormat=description.Format;
  if(!format(sampleFormat)||family(sampleFormat)!=family(description.Format)){invalidate();return false;}
  ID3D11Device*device=NULL;ID3D11Device*owner=NULL;context->GetDevice(&device);source->GetDevice(&owner);
  const bool matching=device&&owner&&device==owner; if(owner)owner->Release();
  if(!matching){if(device)device->Release();invalidate();return false;}
  if(texture_){ID3D11Device*previous=NULL;texture_->GetDevice(&previous);const bool oldDevice=previous!=device;if(previous)previous->Release();
   if(oldDevice){if(eye!=0){device->Release();invalidate();return false;}close();}}
  if(texture_&&(!same(shape_,description)||sampleFormat_!=sampleFormat)){if(eye!=0){device->Release();invalidate();return false;}close();}
  if(!texture_&&!allocate(device,description,sampleFormat)){device->Release();invalidate();return false;}
  context->CopySubresourceRegion(texture_,eye,0,0,0,source,0,NULL);
  const bool healthy=SUCCEEDED(device->GetDeviceRemovedReason());device->Release();
  if(!healthy){invalidate();return false;}
  if(eye==0){frame_=latestFrame_=frame;generation_=generation;mask_=1;}else mask_=3;
  return true;
 }
 bool ready(uint64_t frame,uint64_t generation)const{return mask_==3&&frame&&generation&&frame_==frame&&generation_==generation;}
 // TEXTURE2DARRAY view with one slice: consumer MUST use Texture2DArray.Sample(...,0),
 // not the older prototype's Texture2D shader declaration.
 ID3D11ShaderResourceView*view(unsigned eye,uint64_t frame,uint64_t generation)const{return eye<2&&ready(frame,generation)?eye_[eye]:NULL;}
 ID3D11Texture2D*texture(uint64_t frame,uint64_t generation)const{return ready(frame,generation)?texture_:NULL;}
};
}
