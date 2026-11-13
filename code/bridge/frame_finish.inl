// Actual Session finish/layer selection; included by CPU service doubles too.
    void finish(uint64_t id,uint64_t gen,bool cancel=false) {
        if(!begun){scopeSources.invalidate();demand(cancel||!id,"XR_NO_FRAME");return;}
        const long long perfStart147=Perf147::now();perf147.steps.reset(perfStart147);
        demand(cancel||(!fault&&id==frame&&gen==generation),"XR_FINISH_IDENTITY");
        for(auto&e:eyes)demand(!e.acquired,"XR_UNRELEASED_IMAGE");demand(!panelEye.acquired&&!scopePanelEye.acquired,"XR_UNRELEASED_IMAGE");
        bool submit=!cancel&&visible;demand(!submit||copied==2,"XR_INCOMPLETE_PAIR");
        std::array<XrCompositionLayerProjectionView,2> views{{{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
        for(unsigned e=0;e<2;++e){views[e].pose=rendered[e].pose;views[e].fov=rendered[e].fov;views[e].subImage.swapchain=eyes[e].swap;views[e].subImage.imageRect.extent={int32_t(width),int32_t(height)};}
        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};layer.space=space;layer.viewCount=2;layer.views=views.data();
        const auto*ptr=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer);
        XrCompositionLayerQuad menuLayer{XR_TYPE_COMPOSITION_LAYER_QUAD};
        XrCompositionLayerQuad scopeLayer{XR_TYPE_COMPOSITION_LAYER_QUAD};
        if(scopePanel.active&&submit){
            pauseAnchor.reset();perf147.cpu("scope_panel_layer",[&](){return scope_panel_layer(scopeLayer);});
            ptr=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&scopeLayer);
        }else if(panel&&submit){
            demand(space!=XR_NULL_HANDLE,"XR_MENU_SPACE");
            const GxPose head=pod(shared086::midpoint(pose(rendered[0].pose),pose(rendered[1].pose)));
            demand(pauseAnchor.enter(head),"XR_PAUSE_ANCHOR_POSE");
            const GxPose&anchor=pauseAnchor.pose();menuLayer.space=space;menuLayer.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
            demand(panelEye.swap&&panelWidth&&panelHeight,"XR_PANEL_IMAGE");
            menuLayer.subImage.swapchain=panelEye.swap;menuLayer.subImage.imageArrayIndex=0;menuLayer.subImage.imageRect.offset={0,0};
            menuLayer.subImage.imageRect.extent={int32_t(panelWidth),int32_t(panelHeight)};
            menuLayer.pose.orientation={float(anchor.qx),float(anchor.qy),float(anchor.qz),float(anchor.qw)};
            menuLayer.pose.position={float(anchor.x),float(anchor.y),float(anchor.z)};
            const float wide=2.8f*uiVisibility.pausePercent*.01f;menuLayer.size={wide,wide*float(panelHeight)/float(panelWidth)};
            ptr=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&menuLayer);
        }else if(!panel)pauseAnchor.reset();
        const XrCompositionLayerBaseHeader*layers[16]={ptr};unsigned count=submit?1:0;
        const bool world=submit&&!panel&&!scopePanel.active;
        XrCompositionLayerQuad fadeQuad{XR_TYPE_COMPOSITION_LAYER_QUAD}; // build167 item 4: right above the world
        if(world&&count<uiMaxLayers&&perf147.cpu("fade_layer",[&](){return fade_layer(fadeQuad);}))
            layers[count++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&fadeQuad);
        XrCompositionLayerQuad wrist{XR_TYPE_COMPOSITION_LAYER_QUAD};
        const bool mapGeometry=world&&count<uiMaxLayers&&perf147.cpu("wrist_layer",[&](){return wrist_layer(wrist,true);});
        const bool mapVisible=mapGeometry&&gx_ui_visibility::enabled(uiVisibility,gx_ui_visibility::Map);
        if(mapVisible)layers[count++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&wrist);
        XrCompositionLayerQuad wanted{XR_TYPE_COMPOSITION_LAYER_QUAD};
        if(mapGeometry&&count<uiMaxLayers&&perf147.cpu("wanted_ui_layer",[&](){return wanted_ui_layer(wanted,wrist);}))layers[count++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&wanted);
        XrCompositionLayerQuad weaponHud{XR_TYPE_COMPOSITION_LAYER_QUAD};
        if(world&&count<uiMaxLayers&&perf147.cpu("weapon_hud_layer",[&](){return weapon_hud_layer(weaponHud);}))layers[count++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&weaponHud);
        XrCompositionLayerQuad rightHud{XR_TYPE_COMPOSITION_LAYER_QUAD};
        const bool composeRight=world&&count<uiMaxLayers;
        if(composeRight&&perf147.cpu("native_ui_layer",[&](){return native_ui_layer(rightHud,false,0,right_element_layer_allowance(count));}))
            layers[count++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&rightHud);
        std::array<XrCompositionLayerQuad,gx_ui_element::Count> rightElements{};
        if(composeRight&&!overlayState.visible&&rightPlanFrame==frame&&elementUiFrame==frame&&
           elementUiEye.swap&&!elementUiEye.acquired)for(unsigned k=0;k<rightElements.size();++k){
            rightElements[k].type=XR_TYPE_COMPOSITION_LAYER_QUAD;
            if(!(rightPlan.admittedMask&(1u<<k)))continue;
            demand(count<uiMaxLayers,"UI_ELEMENT_LAYER_PREFLIGHT");
            if(perf147.cpu("native_ui_element_layer",[&](){return native_ui_element_layer(rightElements[k],k);}))layers[count++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&rightElements[k]);
        }
        std::array<XrCompositionLayerQuad,2> handContexts{};
        if(world)for(unsigned h=0;h<handContexts.size();++h){
            handContexts[h].type=XR_TYPE_COMPOSITION_LAYER_QUAD;
            if(count>=uiMaxLayers){if(frame<=5||frame%120==0)log("UI_GAP reason=hand_layer_budget");break;}
            if(perf147.cpu("hand_context_layer",[&](){return hand_context_layer(handContexts[h],h);}))layers[count++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&handContexts[h]);
        }
        XrCompositionLayerQuad buttonsQuad{XR_TYPE_COMPOSITION_LAYER_QUAD}; // build167 item 3: the wrist buttons card
        if(world&&count<uiMaxLayers&&perf147.cpu("buttons_layer",[&](){return buttons_layer(buttonsQuad);}))
            layers[count++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&buttonsQuad);
        std::array<XrCompositionLayerQuad,8> nativePanels{};
        if(world)for(unsigned n=0;n<nativePanels.size();++n){
            nativePanels[n].type=XR_TYPE_COMPOSITION_LAYER_QUAD;
            if(count>=uiMaxLayers){if(frame<=5||frame%120==0)log("UI_GAP reason=runtime_layer_budget");break;}
            if(perf147.cpu("native_ui_layer",[&](){return native_ui_layer(nativePanels[n],true,n);}))layers[count++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&nativePanels[n]);
        }
        XrCompositionLayerQuad options{XR_TYPE_COMPOSITION_LAYER_QUAD};
        if(world&&count<uiMaxLayers&&overlayState.visible){
            if(overlayState.version==3){
                if(perf147.cpu("native_menu_layer",[&](){return native_menu_layer(options);}))layers[count++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&options);
            }else{
            perf147.cpu("upload_overlay",[&](){upload_overlay();});options.space=head_space;options.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
            options.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT|XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
            options.subImage.swapchain=overlayEye.swap;options.subImage.imageRect.extent={gx_ui::Width,gx_ui::Height};
            options.pose.orientation.w=1;options.pose.position={0,0,-1.4f};options.size={1.04f,1.04f*static_cast<float>(gx_ui::Height)/static_cast<float>(gx_ui::Width)};
            layers[count++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&options);
            }
        }
        std::array<XrCompositionLayerQuad,4> opticLayers{};unsigned opticCount=0;
        if(world&&count<uiMaxLayers&&perf147.cpu("scope_optic_layers",[&](){return scope_optic_layers(opticLayers.data(),uiMaxLayers-count,opticCount);}))
            for(unsigned i=0;i<opticCount;++i)layers[count++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&opticLayers[i]);
        XrFrameEndInfo ei{XR_TYPE_FRAME_END_INFO};ei.displayTime=timing.predictedDisplayTime;ei.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;ei.layerCount=count;ei.layers=submit?layers:nullptr;
        health("end_before");perf147.seal(context.Get());check(perf147.cpu("xrEndFrame",[&](){return api.EndFrame(session,&ei);}),"EndFrame");begun=false;scopeSources.invalidate();invalidate_scope_optics(!cancel);health("end_after");if(submit){if(scopePanel.active)++scope_frames;else if(panel)++menu_frames;else ++submitted;}
        perf147.cpuRecord(frame,generation,cancel?"cancel":"finish",timing.predictedDisplayTime,timing.predictedDisplayPeriod,timing.shouldRender!=0,perfStart147,Perf147::now());
        LARGE_INTEGER qpc;QueryPerformanceCounter(&qpc);
        if(!compact_logging||frame<=5||frame%120==0)log("FRAME id="+std::to_string(frame)+" generation="+std::to_string(generation)+" display_time="+std::to_string(timing.predictedDisplayTime)+" qpc="+std::to_string(qpc.QuadPart)+" copies="+std::to_string(copied)+" layers="+std::to_string(ei.layerCount)+" mode="+(scopePanel.active?"SCOPE":panel?"MENU":"WORLD"));
    }
