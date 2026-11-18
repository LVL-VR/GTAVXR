// Included in Session. Copy-only compositor: no shader bindings, native state,
// query/predication, simulation, Scaleform timestep or GPU readback is touched.
struct UiPixels {
    GxUiItem item{};ComPtr<ID3D11Texture2D> texture;uint64_t captured=0,serial=0;
    unsigned sourceWidth=0,sourceHeight=0;
};
std::array<UiPixels,64> uiPixels;
Eye rightUiEye;
Eye elementUiEye;gx_ui_element::Plan rightPlan;uint64_t rightPlanFrame=0,elementUiFrame=0;
Eye subtitlePreviewEye;
ComPtr<ID3D11Texture2D> subtitlePreviewPixels;
bool subtitleLayerLogged=false;
unsigned hudProductReceipts=0;
uint64_t rightUiFrame=0,panelUiFrame=0,uiCaptures=0;
unsigned rightUiWidth=0,rightUiHeight=0;
uint32_t uiMaxLayers=16,uiMaxWidth=4096,uiMaxHeight=4096; // actual runtime properties at initialize
void close_ui_eye(Eye&e){
    if(e.acquired){if(!e.waited){XrSwapchainImageWaitInfo w{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};w.timeout=1000000000;
        demand(perf147.cpu("xrWaitSwapchainImage",[&](){return api.WaitSwapchainImage(e.swap,&w);})==XR_SUCCESS,"UI_CLEANUP_WAIT");e.waited=true;}
        XrSwapchainImageReleaseInfo r{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};check(perf147.cpu("xrReleaseSwapchainImage",[&](){return api.ReleaseSwapchainImage(e.swap,&r);}),"UiCleanupRelease");e.acquired=e.waited=false;}
    if(e.swap){e.images.clear();check(api.DestroySwapchain(e.swap),"UiDestroy");e.swap={};}
}
XrResult try_create_ui_eye(Eye&e,unsigned w,unsigned h){
    XrSwapchainCreateInfo c{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    c.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
    c.format=selected_format;c.sampleCount=c.faceCount=c.arraySize=c.mipCount=1;c.width=w;c.height=h;
    const XrResult created=perf147.cpu("xrCreateSwapchain",[&](){return api.CreateSwapchain(session,&c,&e.swap);});
    if(XR_FAILED(created))return created;
    uint32_t n=0;
    check(perf147.cpu("xrEnumerateSwapchainImages",[&](){return api.EnumerateSwapchainImages(e.swap,0,&n,nullptr);}),"UiImageCount");demand(n&&n<=16,"UI_IMAGES");
    e.images.assign(n,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
    check(api.EnumerateSwapchainImages(e.swap,n,&n,reinterpret_cast<XrSwapchainImageBaseHeader*>(e.images.data())),"UiImages");
    log("UI_SWAPCHAIN width="+std::to_string(w)+" height="+std::to_string(h)+" images="+std::to_string(n)+
        " color_bytes="+std::to_string(uint64_t(w)*h*4*n)); // excludes runtime allocation overhead
    return created;
}
void create_ui_eye(Eye&e,unsigned w,unsigned h){check(try_create_ui_eye(e,w,h),"UiCreate");}
ID3D11Texture2D* acquire_ui_eye(Eye&e,unsigned w,unsigned h,uint32_t* index=nullptr){
    uint32_t n=0;XrSwapchainImageAcquireInfo a{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    check(perf147.cpu("xrAcquireSwapchainImage",[&](){return api.AcquireSwapchainImage(e.swap,&a,&n);}),"UiAcquire");e.acquired=true;
    XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=1000000000;
    demand(perf147.cpu("xrWaitSwapchainImage",[&](){return api.WaitSwapchainImage(e.swap,&wait);})==XR_SUCCESS,"UI_WAIT");e.waited=true;
    demand(n<e.images.size()&&e.images[n].texture,"UI_INDEX");auto*t=e.images[n].texture;
    D3D11_TEXTURE2D_DESC d{};t->GetDesc(&d);ComPtr<ID3D11Device> owner;t->GetDevice(&owner);
    demand(owner.Get()==device.Get()&&d.Width==w&&d.Height==h&&d.ArraySize==1&&d.MipLevels==1&&d.SampleDesc.Count==1&&
        swap_format(d.Format),"UI_TARGET");
    if(index)*index=n;
    return t;
}
void release_ui_eye(Eye&e){
    context->Flush();health("ui_copy");XrSwapchainImageReleaseInfo r{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    check(perf147.cpu("xrReleaseSwapchainImage",[&](){return api.ReleaseSwapchainImage(e.swap,&r);}),"UiRelease");e.acquired=e.waited=false;
}
#include "ui_panel_storage.inl"
bool capture_native_ui(const GxUiCapture&r,ID3D11Texture2D*source){
    demand(!fault&&begun&&visible&&!panel&&source&&r.size==sizeof(r)&&r.version==1&&!r.reserved&&
        r.frame==frame&&r.generation==generation&&r.count<=GX_UI_MAX_ITEMS,"UI_FRAME");
    D3D11_TEXTURE2D_DESC sd{};source->GetDesc(&sd);ComPtr<ID3D11Device> owner;source->GetDevice(&owner);
    demand(owner.Get()==device.Get()&&sd.Width==r.source_width&&sd.Height==r.source_height&&sd.ArraySize==1&&
        sd.MipLevels==1&&sd.SampleDesc.Count==1&&
        (sd.Format==DXGI_FORMAT_B8G8R8A8_UNORM||sd.Format==DXGI_FORMAT_B8G8R8A8_TYPELESS||sd.Format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB),"UI_SOURCE");
    for(unsigned i=0;i<r.count;++i){
        demand(gx_native_ui::bounded(r.items[i],sd.Width,sd.Height),"UI_RECT");
        for(unsigned j=0;j<i;++j)demand(r.items[i].id!=r.items[j].id,"UI_DUPLICATE_ID");
        if(gx_native_ui::wantedItem(r.items[i]))for(const auto&p:uiPixels)
            if(p.captured==frame&&gx_native_ui::wantedItem(p.item)){
                log("UI_GAP reason=wanted_duplicate_same_frame");return false;}
        if(gx_native_ui::handRoute(r.items[i].route)){
            const unsigned hand=gx_native_ui::routeHand(r.items[i].route);
            for(unsigned j=0;j<i;++j)if(gx_native_ui::handRoute(r.items[j].route))
                demand(gx_native_ui::routeHand(r.items[j].route)!=hand,"UI_HAND_DUPLICATE");
            for(const auto&p:uiPixels)if(p.captured==frame&&gx_native_ui::handRoute(p.item.route))
                demand(gx_native_ui::routeHand(p.item.route)!=hand,"UI_HAND_DUPLICATE");
        }
    }
    for(auto&p:uiPixels)demand(p.texture.Get()!=source,"UI_SOURCE_ALIAS");
    // Resolve capacity and allocate ALL destinations before any pixel copy or
    // visible metadata mutation. Rejections leave earlier producers untouched.
    auto staged=uiPixels;std::array<unsigned,GX_UI_MAX_ITEMS> destinations{};
    uint64_t nextSerial=uiCaptures;
    for(unsigned i=0;i<r.count;++i){const auto&it=r.items[i];UiPixels*slot=nullptr;
        for(auto&p:staged)if(p.item.id==it.id){slot=&p;break;}
        if(!slot){for(auto&p:staged)if(!p.item.id){slot=&p;break;}}
        if(!slot){auto oldest=std::min_element(staged.begin(),staged.end(),[](const UiPixels&a,const UiPixels&b){return a.captured<b.captured;});
            if(oldest->captured!=frame)slot=&*oldest;}
        if(!slot){log("UI_GAP reason=batch_component_capacity id="+std::to_string(it.id));return false;}
        // Bound total retained GPU pixels; evict only non-current copies. Never
        // remove a current producer just to hide incomplete simultaneous UI.
        uint64_t pixels=uint64_t(it.width)*it.height;
        for(auto&p:staged)if(&p!=slot&&p.texture)pixels+=uint64_t(p.item.width)*p.item.height;
        if(pixels>20000000){for(auto&p:staged)if(&p!=slot&&p.captured!=frame&&p.texture){
                pixels-=uint64_t(p.item.width)*p.item.height;p=UiPixels();if(pixels<=20000000)break;}}
        if(pixels>20000000){log("UI_GAP reason=batch_pixel_capacity id="+std::to_string(it.id));return false;}
        if(!slot->texture||slot->item.width!=it.width||slot->item.height!=it.height){
            D3D11_TEXTURE2D_DESC d{};d.Width=it.width;d.Height=it.height;d.ArraySize=d.MipLevels=1;
            d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;ComPtr<ID3D11Texture2D> storage;
            demand(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&storage)),"UI_STORAGE");slot->texture=storage;
        }
        destinations[i]=unsigned(slot-staged.data());
        slot->item=it;slot->captured=frame;slot->serial=++nextSerial;slot->sourceWidth=sd.Width;slot->sourceHeight=sd.Height;
    }
    for(unsigned i=0;i<r.count;++i){const auto&it=r.items[i];auto&slot=staged[destinations[i]];
        D3D11_BOX box{it.x,it.y,0,it.x+it.width,it.y+it.height,1};
        perf147.gpu(context.Get(),"ui_capture",-1,int(i),[&](){context->CopySubresourceRegion(slot.texture.Get(),0,0,0,0,source,0,&box);});
    }
    health("ui_batch_copy");uiPixels=std::move(staged);uiCaptures=nextSerial;rightPlanFrame=0;
    const unsigned tracked[]={1,10,11,12,15,GX_UI_FEED};
    for(unsigned k=0;k<sizeof(tracked)/sizeof(tracked[0]);++k){
        if(hudProductReceipts&(1u<<k))continue;
        for(unsigned i=0;i<r.count;++i)if(r.items[i].id==tracked[k]){
            const auto& item=r.items[i];
            hudProductReceipts|=1u<<k;
            log("HUD_PRODUCT frame="+std::to_string(frame)+" id="+std::to_string(item.id)+
                " route="+std::to_string(item.route)+" size="+std::to_string(item.width)+"x"+std::to_string(item.height));
            break;
        }
    }
    // Source pointers are not retained. No readback, frame latency or state setter.
    return true;
}
bool wanted_ui_layer(XrCompositionLayerQuad&q,const XrCompositionLayerQuad* map){
    if(overlayState.visible||panel||!gx_ui_visibility::enabled(uiVisibility,gx_ui_element::Wanted))return false;
    const bool leftTracked=wristHandFrame==frame&&wristHandValid;
    if(!map&&!leftTracked&&!head_space)return false;
    UiPixels*p=nullptr;
    for(auto&v:uiPixels)if(v.texture&&v.captured==frame&&gx_native_ui::wantedItem(v.item)){p=&v;break;}
    if(!p)return false;
    const auto&r=p->item;
    const float leftScale=gx_wrist::scale(wristProfiles.hand[0]);
    const auto&profile=uiElementProfiles.element[gx_ui_element::Wanted];
    const float ownScale=profile.individual?profile.percent*.01f:1.f;
    const bool headFallback=!map&&!leftTracked;
    GxPose anchor={-.34,.30,-1.45,0,0,0,1};
    float width=.32f;
    q.space=headFallback?head_space:space;
    if(map){
        gx_wanted_ui::Row row;
        if(!gx_wanted_ui::row(wristWidth,wristHeight,r.width,r.height,map->size.width,map->size.height,leftScale,row))
            return false;
        anchor={map->pose.position.x,map->pose.position.y,map->pose.position.z,
                map->pose.orientation.x,map->pose.orientation.y,map->pose.orientation.z,map->pose.orientation.w};
        anchor=gx_wanted_ui::below(anchor,(map->size.height+row.height*ownScale)*.5+.008*leftScale);
        width=float(row.width);
        q.space=map->space;
    }else if(leftTracked){
        anchor=gx_wrist::pose(wristGrip,wristProfiles.hand[0],0);
        width=.14f*leftScale;
    }
    if(profile.individual)anchor=gx_ui_element::pose(anchor,profile,headFallback?1.f:leftScale);
    q.pose.orientation={float(anchor.qx),float(anchor.qy),float(anchor.qz),float(anchor.qw)};
    q.pose.position={float(anchor.x),float(anchor.y),float(anchor.z)};
    q.size={width*ownScale,width*ownScale*float(r.height)/float(r.width)};
    if(!headFallback&&!place_panel(q,gx_panel::WANTED))return false;
    if(r.width>uiMaxWidth||r.height>uiMaxHeight){
        if(frame<=5||frame%120==0)log("UI_GAP reason=runtime_wanted_size");return false;}
    if(!publish_ui_panel(q.subImage,wantedUiPanel,*p,256,128,"wanted_copy"))return false;
    q.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
    q.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT|XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
    return true;
}
void place_subtitle(XrCompositionLayerQuad& q){
    const auto& profile=uiElementProfiles.element[gx_ui_element::Subtitles];
    const GxPose front=gx_panel::subtitlePose(profile);
    q.space=head_space;
    q.pose.position={float(front.x),float(front.y),float(front.z)};
    q.pose.orientation={float(front.qx),float(front.qy),float(front.qz),float(front.qw)};
    const float individual=profile.individual?profile.percent*.01f:1.f;
    q.size.width*=individual;q.size.height*=individual;
    const auto& placement=panelPlacements.panel[gx_panel::EL_SUBTITLES];
    if(placement.anchor==gx_panel::DEFAULT||!place_panel(q,gx_panel::EL_SUBTITLES)){
        q.size.width*=placement.scale;q.size.height*=placement.scale;
    }
}
bool subtitle_preview(XrCompositionLayerQuad& q){
    constexpr unsigned w=256,h=40;
    if(!panelPlacements.previewSubtitle||w>uiMaxWidth||h>uiMaxHeight)return false;
    if(!subtitlePreviewPixels){
        std::array<uint32_t,w*h> pixels{};
        for(unsigned y=2;y<h-2;++y)for(unsigned x=2;x<w-2;++x){
            const bool border=x<4||x>=w-4||y<4||y>=h-4;
            const bool line=(y>=12&&y<16&&x>30&&x<w-30)||(y>=24&&y<28&&x>61&&x<w-61);
            pixels[y*w+x]=line?0xffeeeeeeu:border?0xff6acf82u:0xb6202020u;
        }
        D3D11_TEXTURE2D_DESC d{};
        d.Width=w;d.Height=h;d.ArraySize=d.MipLevels=d.SampleDesc.Count=1;
        d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.Usage=D3D11_USAGE_IMMUTABLE;
        d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{};data.pSysMem=pixels.data();data.SysMemPitch=w*4;
        demand(SUCCEEDED(device->CreateTexture2D(&d,&data,&subtitlePreviewPixels)),"UI_SUBTITLE_PREVIEW");
    }
    if(!subtitlePreviewEye.swap)create_ui_eye(subtitlePreviewEye,w,h);
    auto* texture=acquire_ui_eye(subtitlePreviewEye,w,h);
    swap_copy(texture,subtitlePreviewPixels.Get());
    release_ui_eye(subtitlePreviewEye);
    q.subImage.swapchain=subtitlePreviewEye.swap;
    q.subImage.imageRect.extent={int(w),int(h)};
    q.size={.76f,.76f*h/w};
    place_subtitle(q);
    return true;
}
bool native_ui_layer(XrCompositionLayerQuad&q,bool modal,unsigned which=0,unsigned availableRightLayers=1,
                     bool subtitlesOnly=false){
    if(overlayState.visible||panel||!head_space)return false;
    std::vector<UiPixels*> current;
    for(auto&p:uiPixels)if(p.texture&&p.captured==frame&&gx_ui_visibility::product(uiVisibility,p.item)&&!gx_native_ui::handRoute(p.item.route)&&
                         !gx_native_ui::wantedItem(p.item)&&!gx_menyoo_route::belongs(p.item)&&
                         !gx_native_ui::buttonsItem(p.item)&&(p.item.id==15)==subtitlesOnly)current.push_back(&p);
    std::sort(current.begin(),current.end(),[](const UiPixels*a,const UiPixels*b){
        if(a->item.priority!=b->item.priority)return a->item.priority>b->item.priority;return a->item.id<b->item.id;});
    const bool rightFallback=!panel_free(gx_panel::RIGHT)&&(hudHandFrame!=frame||!(hudHandMask&2));
    gx_native_ui::Layout layout;std::vector<std::pair<UiPixels*,gx_native_ui::Placement>> cards;std::vector<UiPixels*> large;
    std::vector<UiPixels*> overflow;
    for(auto*p:current){
        if(gx_native_ui::fitRoute(p->item)==GX_UI_PANEL){large.push_back(p);continue;}
        if(uiMaxWidth>=GX_UI_ATLAS&&uiMaxHeight>=GX_UI_ATLAS){
            auto dst=layout.append(p->item);
            if(dst.placed){cards.push_back({p,dst});continue;}
        }
        if(p->item.width<=uiMaxWidth&&p->item.height<=uiMaxHeight)overflow.push_back(p);
        else if(frame<=5||frame%120==0)log("UI_GAP reason=runtime_hud_size id="+std::to_string(p->item.id));
    }
    q.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
    q.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT|XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
    if(modal){
        // Native modal order/relative placement, not a priority sort that could
        // put a background over its foreground. Multiple simultaneous producers
        // get distinct layers; they are not silently replaced by the first one.
        std::sort(large.begin(),large.end(),[](const UiPixels*a,const UiPixels*b){return a->serial<b->serial;});
        const size_t nativeCount=large.size();
        large.insert(large.end(),overflow.begin(),overflow.end());
        const size_t modalLimit=uiPanels.size();
        if(which>=large.size()||which>=modalLimit)return subtitlesOnly&&subtitle_preview(q);
        if(which==0&&large.size()>modalLimit&&(frame<=5||frame%120==0))log("UI_GAP reason=modal_capacity count="+std::to_string(large.size()));
        UiPixels*p=large[which];const auto&r=p->item;auto&modalEye=subtitlesOnly?subtitleUiPanel:uiPanels[which];
        if(r.width>uiMaxWidth||r.height>uiMaxHeight){if(frame<=5||frame%120==0)log("UI_GAP reason=runtime_panel_size id="+std::to_string(r.id));return false;}
        if(!publish_ui_panel(q.subImage,modalEye,*p,subtitlesOnly?1024:512,subtitlesOnly?128:1024,"modal_copy"))return false;
        panelUiFrame=frame;
        q.space=head_space;
        q.pose.orientation.w=1;
        if(which>=nativeCount){
            const size_t index=which-nativeCount;
            const float aspect=float(r.height)/float(r.width);
            const float width=(std::min)(.37f,.14f/aspect);
            q.size={width,width*aspect};
            q.pose.position={.53f-.42f*float(index/4),.48f-.17f*float(index%4),-1.45f};
            return true;
        }
        if(r.route==GX_UI_MENU_PANEL){
            // Recompose only the native menu crops about THEIR shared centre,
            // keeping relative text/background/footer placement, not16:9 edges.
            gx_menu_panel::Region region;
            for(const auto*part:large)if(part->item.route==GX_UI_MENU_PANEL)
                region.add(part->item,part->sourceWidth,part->sourceHeight);
            gx_menu_panel::Quad placement;
            demand(gx_menu_panel::place(region,r,p->sourceWidth,p->sourceHeight,placement,uiElementProfiles.menuPercent),"UI_MENU_PLACEMENT");
            q.size={float(placement.width),float(placement.height)};
            q.pose.position={float(placement.x),float(placement.y),-1.2f};return true;
        }
        const float screenWidth=1.55f,screenHeight=screenWidth*float(p->sourceHeight)/p->sourceWidth;
        q.size={screenWidth*float(r.width)/p->sourceWidth,screenHeight*float(r.height)/p->sourceHeight};
        q.pose.position={screenWidth*((r.x+r.width*.5f)/p->sourceWidth-.5f),
            screenHeight*(.5f-(r.y+r.height*.5f)/p->sourceHeight),-1.45f};
        if(subtitlesOnly){ // same anchor and scale as the positioning preview
            if(!subtitleLayerLogged){
                subtitleLayerLogged=true;
                log("SUBTITLE_LAYER frame="+std::to_string(frame)+" generation="+std::to_string(generation)+
                    " size="+std::to_string(r.width)+"x"+std::to_string(r.height));
            }
            place_subtitle(q);
        }return true;
    }
    if(cards.empty())return false;
    if(rightPlanFrame!=frame){
        GxUiItem items[gx_ui_element::MaxCards];demand(cards.size()<=gx_ui_element::MaxCards,"UI_ELEMENT_CARD_CAPACITY");
        for(unsigned i=0;i<cards.size();++i)items[i]=cards[i].first->item;
        rightPlan=gx_ui_element::plan(items,unsigned(cards.size()),
            rightFallback?gx_ui_element::defaults():uiElementProfiles,availableRightLayers);
        demand(rightPlan.count==cards.size(),"UI_ELEMENT_PLAN");
        if(rightPlan.rejectedMask&&(frame<=5||frame%120==0))
            log("UI_GAP reason=element_layer_budget_automatic_fallback mask="+std::to_string(rightPlan.rejectedMask));
        // Preflight both placements before the first copy. A rejected custom
        // category is NOT removed from automatic native presentation.
        auto upload=[&](Eye&eye,bool individual){
            if(!eye.swap)create_ui_eye(eye,GX_UI_ATLAS,GX_UI_ATLAS);
            auto*t=acquire_ui_eye(eye,GX_UI_ATLAS,GX_UI_ATLAS);ComPtr<ID3D11RenderTargetView> clear;
            D3D11_RENDER_TARGET_VIEW_DESC rd{};rd.Format=(DXGI_FORMAT)selected_format;rd.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
            demand(SUCCEEDED(device->CreateRenderTargetView(t,&rd,&clear)),"UI_ATLAS_RTV");
            const float zero[4]={0,0,0,0};perf147.gpu(context.Get(),"atlas_clear",-1,individual?1:0,[&](){context->ClearRenderTargetView(clear.Get(),zero);});
            for(unsigned i=0;i<cards.size();++i){
                const bool admitted=(rightPlan.admittedMask&(1u<<rightPlan.kinds[i]))!=0;
                if(admitted!=individual)continue;const auto&dst=individual?rightPlan.custom[i]:rightPlan.original[i];
                demand(dst.placed&&t!=cards[i].first->texture.Get(),"UI_ATLAS_ALIAS_OR_PLAN");
                perf147.gpu(context.Get(),individual?"element_card_copy":"right_card_copy",-1,int(i),[&](){swap_copy(t,cards[i].first->texture.Get(),dst.x,dst.y,nullptr,true);});
            }
            release_ui_eye(eye);
        };
        if(rightPlan.legacy){upload(rightUiEye,false);rightUiFrame=frame;}
        if(rightPlan.admittedMask){upload(elementUiEye,true);elementUiFrame=frame;}
        rightUiWidth=rightPlan.legacyWidth;rightUiHeight=rightPlan.legacyHeight;rightPlanFrame=frame;
    }
    if(!rightPlan.legacy||rightUiFrame!=frame)return false;
    q.space=space;q.subImage.swapchain=rightUiEye.swap;q.subImage.imageRect.extent={int(rightUiWidth),int(rightUiHeight)};
    const GxPose adjusted=gx_wrist::pose(hudGrip[1],wristProfiles.hand[1],1);
    q.pose.orientation={float(adjusted.qx),float(adjusted.qy),float(adjusted.qz),float(adjusted.qw)};
    q.pose.position={float(adjusted.x),float(adjusted.y),float(adjusted.z)};
    const float aspect=float(rightUiHeight)/float(rightUiWidth);
    const float wide=gx_cash_ui::wristWidth(rightUiWidth,rightUiHeight,rightPlan.moneyOnly)*gx_wrist::scale(wristProfiles.hand[1]);
    q.size={wide,wide*aspect};
    if(!rightFallback&&place_panel(q,gx_panel::RIGHT))return true;
    const float frontWidth=(std::min)(.48f,.59f/aspect);
    q.space=head_space;q.pose.orientation={0,0,0,1};
    q.pose.position={.43f,.1f,-1.45f};q.size={frontWidth,frontWidth*aspect};
    return true;
}
bool native_ui_element_layer(XrCompositionLayerQuad&q,unsigned kind){
    if(kind>=gx_ui_element::Count||overlayState.visible||panel||!space||rightPlanFrame!=frame||
       elementUiFrame!=frame||!elementUiEye.swap||elementUiEye.acquired||
       (!panel_free(gx_panel::elementPanel(kind))&&(hudHandFrame!=frame||!(hudHandMask&2)))||!(rightPlan.admittedMask&(1u<<kind)))return false;
    const auto&g=rightPlan.groups[kind];if(!g.count||!g.width||!g.height)return false;
    const float parentScale=gx_wrist::scale(wristProfiles.hand[1]);
    const auto&profile=uiElementProfiles.element[kind];
    const GxPose anchor=gx_wrist::pose(hudGrip[1],wristProfiles.hand[1],1);
    const GxPose adjusted=gx_ui_element::pose(anchor,profile,parentScale);
    q.space=space;q.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
    q.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT|XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
    q.subImage.swapchain=elementUiEye.swap;
    q.subImage.imageRect.offset={int(g.x),int(g.y)};q.subImage.imageRect.extent={int(g.width),int(g.height)};
    q.pose.orientation={float(adjusted.qx),float(adjusted.qy),float(adjusted.qz),float(adjusted.qw)};
    q.pose.position={float(adjusted.x),float(adjusted.y),float(adjusted.z)};
    const float wide=gx_ui_element::width(kind,g.width,g.height,profile,parentScale);
    q.size={wide,wide*float(g.height)/g.width};return place_panel(q,gx_panel::elementPanel(kind));
}
void close_native_ui(){
    close_ui_panel(subtitleUiPanel);
    close_ui_eye(subtitlePreviewEye);subtitlePreviewPixels.Reset();
    close_native_menu_layer();
    close_ui_panel(wantedUiPanel);
    close_ui_eye(rightUiEye);close_ui_eye(elementUiEye);for(auto& p:uiPanels)close_ui_panel(p);for(auto&p:uiPixels)p=UiPixels();
    rightUiFrame=panelUiFrame=0;rightUiWidth=rightUiHeight=0;
    rightPlanFrame=elementUiFrame=0;rightPlan=gx_ui_element::Plan();
}

#include "gpu_menu_layer.inl"
