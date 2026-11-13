#pragma once
// Native reticle pixels transported once; no movie update/draw or HUD replica.
#include <d3d11.h>
#include <stdint.h>
#include <string.h>
enum { GX_SCOPE_RETICLE_STRAIGHT=1u, GX_SCOPE_RETICLE_PREMULTIPLIED=2u };
#pragma pack(push,8)
struct GxScopeReticleCapture {
 uint32_t size,version,reticleHash,alphaMode;
 uint64_t frame,generation;
 uint32_t x,y,width,height;
};
struct GxScopeReticleBinding {
 uint32_t size,version,hand,reticleHash;
 uint64_t frame,generation;
};
#pragma pack(pop)
namespace gx_scope_reticle {
class Cache {
 struct Entry {
  ID3D11Texture2D*texture;ID3D11ShaderResourceView*view;uint64_t frame,generation;
  unsigned hash,alpha,width,height;DXGI_FORMAT format;
  Entry():texture(NULL),view(NULL),frame(0),generation(0),hash(0),alpha(0),width(0),height(0),format(DXGI_FORMAT_UNKNOWN){}
  void invalidate(){frame=generation=0;hash=alpha=0;}
  void close(){invalidate();if(view){view->Release();view=NULL;}if(texture){texture->Release();texture=NULL;}width=height=0;format=DXGI_FORMAT_UNKNOWN;}
 }entries_[2];
 Cache(const Cache&);Cache&operator=(const Cache&);
 static DXGI_FORMAT family(DXGI_FORMAT f){
  if(f==DXGI_FORMAT_B8G8R8A8_TYPELESS||f==DXGI_FORMAT_B8G8R8A8_UNORM||f==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)return DXGI_FORMAT_B8G8R8A8_TYPELESS;
  if(f==DXGI_FORMAT_R8G8B8A8_TYPELESS||f==DXGI_FORMAT_R8G8B8A8_UNORM||f==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)return DXGI_FORMAT_R8G8B8A8_TYPELESS;
  return DXGI_FORMAT_UNKNOWN;
 }
public:
 Cache(){}~Cache(){close();}
 void invalidate(){for(unsigned i=0;i<2;++i)entries_[i].invalidate();}
 void close(){for(unsigned i=0;i<2;++i)entries_[i].close();}
 bool capture(ID3D11DeviceContext*context,ID3D11Texture2D*source,const GxScopeReticleCapture&r,DXGI_FORMAT sampleFormat){
  if(!context||!source||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||r.size!=sizeof(r)||r.version!=1||!r.frame||!r.generation||!r.reticleHash||
     (r.alphaMode!=GX_SCOPE_RETICLE_STRAIGHT&&r.alphaMode!=GX_SCOPE_RETICLE_PREMULTIPLIED)||!r.width||!r.height||r.width>3840||r.height>2160)return false;
  if(sampleFormat!=DXGI_FORMAT_B8G8R8A8_UNORM&&sampleFormat!=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB&&
     sampleFormat!=DXGI_FORMAT_R8G8B8A8_UNORM&&sampleFormat!=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)return false;
  D3D11_TEXTURE2D_DESC s={};source->GetDesc(&s);
  if(s.ArraySize!=1||s.MipLevels!=1||s.SampleDesc.Count!=1||s.SampleDesc.Quality||s.Usage!=D3D11_USAGE_DEFAULT||s.CPUAccessFlags||
     (s.MiscFlags&(D3D11_RESOURCE_MISC_SHARED|D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX))||family(s.Format)!=family(sampleFormat)||
     r.x>s.Width||r.y>s.Height||r.width>s.Width-r.x||r.height>s.Height-r.y)return false;
  ID3D11Device*device=NULL,*owner=NULL;context->GetDevice(&device);source->GetDevice(&owner);
  bool matching=device&&owner&&device==owner;if(owner)owner->Release();if(!matching){if(device)device->Release();return false;}
  Entry*slot=NULL;
  for(unsigned i=0;i<2;++i){Entry&e=entries_[i];if(e.texture==source){device->Release();return false;}
   if(e.frame==r.frame&&e.generation==r.generation&&e.hash==r.reticleHash){device->Release();return false;}
   if(!slot&&(e.frame!=r.frame||e.generation!=r.generation))slot=&e;
  }
  if(!slot){device->Release();return false;}
  if(slot->texture){slot->texture->GetDevice(&owner);matching=owner==device;if(owner)owner->Release();
   if(!matching||slot->width!=r.width||slot->height!=r.height||slot->format!=sampleFormat)slot->close();}
  if(!slot->texture){D3D11_TEXTURE2D_DESC d={};d.Width=r.width;d.Height=r.height;d.ArraySize=d.MipLevels=1;d.SampleDesc.Count=1;
   d.Format=family(sampleFormat);d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
   D3D11_SHADER_RESOURCE_VIEW_DESC v={};v.Format=sampleFormat;v.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;v.Texture2D.MipLevels=1;
   if(FAILED(device->CreateTexture2D(&d,NULL,&slot->texture))||FAILED(device->CreateShaderResourceView(slot->texture,&v,&slot->view))){slot->close();device->Release();return false;}
   slot->width=r.width;slot->height=r.height;slot->format=sampleFormat;
  }
  D3D11_BOX box={r.x,r.y,0,r.x+r.width,r.y+r.height,1};context->CopySubresourceRegion(slot->texture,0,0,0,0,source,0,&box);
  const bool healthy=SUCCEEDED(device->GetDeviceRemovedReason());device->Release();if(!healthy){slot->invalidate();return false;}
  slot->frame=r.frame;slot->generation=r.generation;slot->hash=r.reticleHash;slot->alpha=r.alphaMode;return true;
 }
 ID3D11ShaderResourceView*view(unsigned hash,uint64_t frame,uint64_t generation,unsigned&alpha)const{
  alpha=0;if(!hash||!frame||!generation)return NULL;
  for(unsigned i=0;i<2;++i){const Entry&e=entries_[i];if(e.hash==hash&&e.frame==frame&&e.generation==generation){alpha=e.alpha;return e.view;}}
  return NULL;
 }
};
}
