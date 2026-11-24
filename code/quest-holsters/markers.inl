// A small stereo quad per slot, sharing a single static D3D11 texture atlas.
PFN_xrEndFrame endFrame;
PFN_xrDestroySession destroySession;
PFN_xrCreateSwapchain createSwap;
PFN_xrDestroySwapchain destroySwap;
PFN_xrEnumerateSwapchainImages images;
PFN_xrAcquireSwapchainImage acquire;
PFN_xrWaitSwapchainImage waitImage;
PFN_xrReleaseSwapchainImage releaseImage;
PFN_xrGetSystemProperties systemProps;
struct MarkerFrame {XrSession session;XrSpace space;XrTime time;XrVector3f head;float c,s;};
MarkerFrame markerFrame{};
XrSwapchain atlas{};
XrSession owner{};
unsigned layerLimit=0;
constexpr int pixels=48;
#pragma comment(lib,"gdi32.lib")
#include "card-icons.inl"
#include "wheel-render.inl"
struct D3DImage {XrStructureType type;void* next;ID3D11Texture2D* texture;};
bool makeAtlas(XrSession session
 ,const WheelState& snapshot
 ){
 XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
 ci.createFlags=XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT;ci.usageFlags=XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
 ci.format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;ci.sampleCount=ci.arraySize=ci.faceCount=ci.mipCount=1;
 ci.width=atlasWidth;ci.height=atlasHeight;
 ci.createFlags=0;
 if(!atlas && XR_FAILED(createSwap(session,&ci,&atlas)))return false;
 uint32_t count=0,index=0;
 if(XR_FAILED(images(atlas,0,&count,nullptr))||!count)return false;
 std::vector<D3DImage> textures(count,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR,nullptr,nullptr});
 if(XR_FAILED(images(atlas,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(textures.data()))))return false;
 XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
 XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wi.timeout=XR_INFINITE_DURATION;
 std::vector<uint32_t> data(atlasWidth*atlasHeight);
 if(!paintWheel(data,snapshot))return false;
 if(XR_FAILED(acquire(atlas,&ai,&index))||XR_FAILED(waitImage(atlas,&wi)))return false;
 ID3D11Device* dev=nullptr;ID3D11DeviceContext* ctx=nullptr;auto tex=textures[index].texture;
 tex->GetDevice(&dev);dev->GetImmediateContext(&ctx);
 ctx->UpdateSubresource(tex,0,nullptr,data.data(),atlasWidth*4,0);ctx->Release();dev->Release();
 XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
 if(XR_FAILED(releaseImage(atlas,&ri)))return false;
 uploadedWheel=snapshot;
 return true;
}
XrPosef markerAt(const MarkerFrame& f,XrVector3f position){
 float x=position.x,y=position.y,z=position.z,dx=f.c*x+f.s*z,dz=-f.s*x+f.c*z;
 // Rotate the quad's +Z normal towards the eyes, including when looking down.
 float len=sqrtf(dx*dx+y*y+dz*dz),w=1-dz/len;
 if(w<.000001f)return {{0,1,0,0},{f.head.x+dx,f.head.y+y,f.head.z+dz}};
 float norm=sqrtf(2*w);
 return {{y/len/norm,-dx/len/norm,0,w/norm},{f.head.x+dx,f.head.y+y,f.head.z+dz}};
}
XrResult XRAPI_CALL markerEnd(XrSession session,const XrFrameEndInfo* info){
 MarkerFrame f;WheelState wheel;Layout positions{};
 {
    std::lock_guard guard(lock);auto now=GetTickCount64();f=markerFrame;
    if(!inventoryOpen||!heartbeat||now-heartbeat>250)f.session={};
    else {wheel=wheelState(now);if(editor)positions=draftLayout;}
 }
 if(f.session!=session||!f.session||std::abs(f.time-info->displayTime)>100000000LL||!info->layerCount||info->layerCount>64||info->layerCount>=layerLimit)return endFrame(session,info);
 if(owner!=session||!atlas||!(wheel==uploadedWheel)){
    owner=session;if(!makeAtlas(session,wheel))return endFrame(session,info);
 }
 XrCompositionLayerQuad q{XR_TYPE_COMPOSITION_LAYER_QUAD};q.space=f.space;
 q.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT|XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
 q.eyeVisibility=XR_EYE_VISIBILITY_BOTH;q.pose=markerAt(f,{0,wheelY,wheelZ});
 float halfYaw=atan2f(f.s,f.c)*.5f;q.pose.orientation={0,sinf(halfYaw),0,cosf(halfYaw)};
 q.size={atlasWidth*metresPerPixel,wheelHeight*metresPerPixel};q.subImage.swapchain=atlas;q.subImage.imageRect={{0,0},{atlasWidth,wheelHeight}};
 std::array<XrCompositionLayerQuad,4> orbs;
 const XrCompositionLayerBaseHeader* list[69];std::copy_n(info->layers,info->layerCount,list);
 list[info->layerCount]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&q);
 auto copy=*info;copy.layers=list;++copy.layerCount;
 if(wheel.items[8]==doneCard){
    for(size_t i=0;i<orbs.size()&&copy.layerCount<layerLimit;i++){
       auto& orb=orbs[i];orb=q;
       orb.pose=markerAt(f,positions[i]);orb.size={.040f,.040f};
       orb.subImage.imageRect={{0,wheelHeight},{pixels,pixels}};
       list[copy.layerCount++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&orb);
    }
 }
 return endFrame(session,&copy);
}
XrResult XRAPI_CALL markerDestroy(XrSession session){
 if(owner==session){if(atlas)destroySwap(atlas);atlas={};owner={};}
 {std::lock_guard guard(lock);if(markerFrame.session==session){markerFrame={};
 confirmations={};
 }}
 return destroySession(session);
}
PFN_xrCreateSession realCreateSession;
XrResult XRAPI_CALL markerCreate(XrInstance i,const XrSessionCreateInfo* info,XrSession* out){
 auto r=realCreateSession(i,info,out);if(XR_SUCCEEDED(r)){
 XrSystemProperties p{XR_TYPE_SYSTEM_PROPERTIES};layerLimit=XR_SUCCEEDED(systemProps(i,info->systemId,&p))?p.graphicsProperties.maxLayerCount:0;}return r;
}
