#pragma once
// Owns ONLY optical images/resources. No world/HMD projection, simulation or
// native target writes. Caller holds the ENGINE immediate-context mutex.
// Context-state swapping preserves native bindings/CB ranges/class instances;
// it does NOT suspend asynchronous queries. Use a qualified end-frame boundary.
#include <d3d11_1.h>
#include <stdint.h>
#include <float.h>
#include "scope_source_cache.h"
namespace gx_scope_crop {
template<class T> inline void release(T*&value){if(value){value->Release();value=NULL;}}
class StateScope {
 ID3D11DeviceContext1*context_;ID3DDeviceContextState*previous_;
 StateScope(const StateScope&);StateScope&operator=(const StateScope&);
public:
 StateScope(ID3D11DeviceContext1*context,ID3DDeviceContextState*owned):context_(context),previous_(NULL){
  context_->SwapDeviceContextState(owned,&previous_);
 }
 ~StateScope(){if(previous_){context_->SwapDeviceContextState(previous_,NULL);previous_->Release();}}
 bool valid()const{return previous_!=NULL;}
};
class Renderer {
 ID3D11Device*device_;ID3DDeviceContextState*state_;
 ID3D11VertexShader*vs_;ID3D11PixelShader*ps_;ID3D11Buffer*constants_;
 ID3D11SamplerState*sampler_;ID3D11RasterizerState*raster_;ID3D11DepthStencilState*depth_;
 ID3D11Texture2D*image_[2];ID3D11RenderTargetView*target_[2];
 uint64_t frame_,generation_;unsigned mask_,extent_;bool single_; // build141 E: monocular = one eye image only
 Renderer(const Renderer&);Renderer&operator=(const Renderer&);
public:
 Renderer():device_(NULL),state_(NULL),vs_(NULL),ps_(NULL),constants_(NULL),sampler_(NULL),raster_(NULL),depth_(NULL),
  frame_(0),generation_(0),mask_(0),extent_(0),single_(false){for(unsigned i=0;i<2;++i){image_[i]=NULL;target_[i]=NULL;}}
 ~Renderer(){close();}
 void invalidate(){frame_=generation_=0;mask_=0;single_=false;}
 void close(){invalidate();for(unsigned i=0;i<2;++i){release(target_[i]);release(image_[i]);}
  release(depth_);release(raster_);release(sampler_);release(constants_);release(ps_);release(vs_);release(state_);release(device_);extent_=0;}
 bool initialize(ID3D11Device*device,const void*vsBytes,size_t vsLength,const void*psBytes,size_t psLength,DXGI_FORMAT format,unsigned extent=512){
  if(device_||!device||!vsBytes||!vsLength||!psBytes||!psLength||extent<32||extent>2048||
    (device->GetCreationFlags()&D3D11_CREATE_DEVICE_SINGLETHREADED)||
    (format!=DXGI_FORMAT_B8G8R8A8_UNORM&&format!=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB&&
     format!=DXGI_FORMAT_R8G8B8A8_UNORM&&format!=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB))return false;
  ID3D11Device1*device1=NULL;if(FAILED(device->QueryInterface(__uuidof(ID3D11Device1),(void**)&device1)))return false;
  const D3D_FEATURE_LEVEL level=device->GetFeatureLevel();
  const HRESULT created=device1->CreateDeviceContextState(0,&level,1,D3D11_SDK_VERSION,__uuidof(ID3D11Device1),NULL,&state_);
  device1->Release();if(FAILED(created))return false;
  device_=device;device_->AddRef();extent_=extent;
  bool ok=SUCCEEDED(device->CreateVertexShader(vsBytes,vsLength,NULL,&vs_))&&
          SUCCEEDED(device->CreatePixelShader(psBytes,psLength,NULL,&ps_));
  D3D11_BUFFER_DESC buffer={};buffer.ByteWidth=48;buffer.Usage=D3D11_USAGE_DEFAULT;buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
  ok=ok&&SUCCEEDED(device->CreateBuffer(&buffer,NULL,&constants_));
  D3D11_SAMPLER_DESC sampler={};sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sampler.MaxLOD=D3D11_FLOAT32_MAX;
  ok=ok&&SUCCEEDED(device->CreateSamplerState(&sampler,&sampler_));
  D3D11_RASTERIZER_DESC raster={};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
  ok=ok&&SUCCEEDED(device->CreateRasterizerState(&raster,&raster_));
  D3D11_DEPTH_STENCIL_DESC depth={};depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;
  ok=ok&&SUCCEEDED(device->CreateDepthStencilState(&depth,&depth_));
  D3D11_TEXTURE2D_DESC shape={};shape.Width=shape.Height=extent;shape.MipLevels=shape.ArraySize=1;
  shape.Format=format;shape.SampleDesc.Count=1;shape.Usage=D3D11_USAGE_DEFAULT;shape.BindFlags=D3D11_BIND_RENDER_TARGET;
  for(unsigned eye=0;ok&&eye<2;++eye)ok=SUCCEEDED(device->CreateTexture2D(&shape,NULL,&image_[eye]))&&
     SUCCEEDED(device->CreateRenderTargetView(image_[eye],NULL,&target_[eye]));
  if(!ok){close();return false;}return true;
 }
 bool draw(ID3D11DeviceContext*context,const gx_scope_source::Cache&source,unsigned eye,uint64_t frame,uint64_t generation,const float*mapping,ID3D11ShaderResourceView*reticle=NULL,unsigned alphaMode=0,bool monocular=false){
  if(!device_||!context||!mapping||eye>=2||!frame||!generation||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE){invalidate();return false;}
  for(unsigned i=0;i<12;++i)if(!_finite(mapping[i])){invalidate();return false;}
  ID3D11ShaderResourceView*view=source.view(eye,frame,generation);if(!view){invalidate();return false;}
  ID3D11Device*owner=NULL;context->GetDevice(&owner);bool matches=owner==device_;release(owner);
  view->GetDevice(&owner);matches=matches&&owner==device_;release(owner);if(!matches){invalidate();return false;}
  float constants[12];memcpy(constants,mapping,sizeof(constants));constants[3]=constants[7]=1;constants[11]=0;
  if(reticle){
   if(alphaMode!=1&&alphaMode!=2){invalidate();return false;}
   reticle->GetDevice(&owner);matches=owner==device_;release(owner);
   D3D11_SHADER_RESOURCE_VIEW_DESC description={};reticle->GetDesc(&description);
   ID3D11Resource*resource=NULL;reticle->GetResource(&resource);ID3D11Texture2D*texture=NULL;
   if(resource)resource->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&texture);release(resource);
   D3D11_TEXTURE2D_DESC shape={};if(texture)texture->GetDesc(&shape);release(texture);
   if(!matches||description.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D||description.Texture2D.MostDetailedMip||description.Texture2D.MipLevels!=1||
      !shape.Width||!shape.Height||shape.Width>3840||shape.Height>2160||shape.ArraySize!=1||shape.MipLevels!=1||shape.SampleDesc.Count!=1){invalidate();return false;}
   constants[3]=shape.Width<shape.Height?(float)shape.Height/shape.Width:1;
   constants[7]=shape.Height<shape.Width?(float)shape.Width/shape.Height:1;constants[11]=(float)alphaMode;
  }
  if(monocular){frame_=frame;generation_=generation;mask_=0;single_=true;} // build141 E: exactly one eye this frame
  else if(eye==0){frame_=frame;generation_=generation;mask_=0;single_=false;}else if(single_||frame_!=frame||generation_!=generation||mask_!=1){invalidate();return false;}
  ID3D11DeviceContext1*context1=NULL;if(FAILED(context->QueryInterface(__uuidof(ID3D11DeviceContext1),(void**)&context1))){invalidate();return false;}
  bool ok=false;{
   StateScope saved(context1,state_);if(saved.valid()){
    // Owned state may contain bindings from our previous eye. Reset ONLY it.
    context1->ClearState();
    context1->UpdateSubresource(constants_,0,NULL,constants,0,0);
    D3D11_VIEWPORT viewport={};viewport.Width=viewport.Height=(float)extent_;viewport.MaxDepth=1;
    context1->RSSetViewports(1,&viewport);context1->RSSetState(raster_);
    context1->OMSetRenderTargets(1,&target_[eye],NULL);context1->OMSetDepthStencilState(depth_,0);
    context1->IASetInputLayout(NULL);context1->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context1->VSSetShader(vs_,NULL,0);context1->PSSetShader(ps_,NULL,0);
    context1->PSSetConstantBuffers(0,1,&constants_);context1->PSSetSamplers(0,1,&sampler_);
    context1->PSSetShaderResources(0,1,&view);context1->PSSetShaderResources(1,1,&reticle);context1->Draw(3,0);
    // Release all scope/native-source references retained by our state object.
    context1->ClearState();ok=true;
   }
  }context1->Release();
  ok=ok&&SUCCEEDED(device_->GetDeviceRemovedReason());if(!ok){invalidate();return false;}
  mask_|=1u<<eye;return true;
 }
 // build141 E: which KIND of image is current, so a monocular crop never stands in for a physical pair (or vice versa).
 bool current(bool monocular,unsigned eye,uint64_t frame,uint64_t generation)const{
  return frame&&frame_==frame&&generation_==generation&&(monocular?(single_&&eye<2&&mask_==(1u<<eye)):(!single_&&mask_==3));
 }
 ID3D11Texture2D*image(unsigned eye,uint64_t frame,uint64_t generation)const{
  return eye<2&&(single_?mask_==(1u<<eye):mask_==3)&&frame_==frame&&generation_==generation?image_[eye]:NULL;
 }
};
}
