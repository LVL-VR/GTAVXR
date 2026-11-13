#pragma once
 
// and the copy into swapchains of any 8-bit colour family. GTAVXR-owned; NGX interface facts only (ngx_iface.h).
//
// Blitter: a full-screen draw (exact Load or bilinear) from a BGRA/RGBA 8-bit or RGBA16F source into a BGRA or RGBA swapchain
//   image, keeping display-encoded bytes (UNORM view of a typeless image; sRGB view + linearize otherwise). Runs in its own
//   device-context state (SwapDeviceContextState), so the game's bindings are untouched (same pattern as the scope crop).
// Chain: per eye, a D3D11 compute capture (colour BGRA8 -> RGBA16F, depth -> R32F, camera motion RG16F from the
//   camera-relative guides) into textures the game's D3D11 device creates and shares; a private D3D12 device on the same
//   adapter waits on a shared fence, runs NR (feature 18, render size) and/or DLSS SR (feature 1, render -> output) through our
//   forwarder copies, signals; D3D11 waits on the GPU and draws the result into the swapchain image. Any chain failure turns
//   the session into the pure D3D11 bilinear blit and records the stage; the game keeps running.
#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <functional>
#include "eye_chain.h"
#include "ngx_iface.h"
#include "eye_chain_cs_bytes.h"
#include "eye_chain_vs_bytes.h"
#include "eye_chain_ps_bytes.h"

namespace gx_chain {
using Microsoft::WRL::ComPtr;

struct Error { const char* stage; long code; };
inline void Hr(HRESULT hr, const char* stage) { if(FAILED(hr)) throw Error{stage, (long)hr}; }
inline void Need(bool ok, const char* stage) { if(!ok) throw Error{stage, 0}; }

enum Family { OTHER = 0, BGRA8 = 1, RGBA8 = 2, RGBA16F = 3 };
inline Family FamilyOf(DXGI_FORMAT f)
{
    switch(f) {
    case DXGI_FORMAT_B8G8R8A8_TYPELESS: case DXGI_FORMAT_B8G8R8A8_UNORM: case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return BGRA8;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return RGBA8;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: case DXGI_FORMAT_R16G16B16A16_FLOAT: return RGBA16F;
    default: return OTHER;
    }
}
// The view that keeps display-encoded values: typeless/UNORM -> UNORM (no conversion); sRGB -> sRGB view, and the shader
// linearizes first so the hardware encode gives the same bytes back.
inline DXGI_FORMAT TargetView(DXGI_FORMAT f, bool& linearize)
{
    linearize = f == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || f == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    if(linearize) return f;
    return FamilyOf(f) == BGRA8 ? DXGI_FORMAT_B8G8R8A8_UNORM : FamilyOf(f) == RGBA8 ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_UNKNOWN;
}
// Source views read raw stored values (UNORM, or the float format) so nothing is decoded twice.
inline DXGI_FORMAT SourceView(DXGI_FORMAT f)
{
    return FamilyOf(f) == BGRA8 ? DXGI_FORMAT_B8G8R8A8_UNORM : FamilyOf(f) == RGBA8 ? DXGI_FORMAT_R8G8B8A8_UNORM :
           FamilyOf(f) == RGBA16F ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_UNKNOWN;
}
// Scene depth view: the game's D32FS8 (typeless) plus the other single-sample depth layouts a renderer may use
inline DXGI_FORMAT DepthView(DXGI_FORMAT f)
{
    return f == DXGI_FORMAT_R32G8X24_TYPELESS ? DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS : f == DXGI_FORMAT_R32_TYPELESS ? DXGI_FORMAT_R32_FLOAT :
           f == DXGI_FORMAT_R24G8_TYPELESS ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : DXGI_FORMAT_UNKNOWN;
}

class StateScope {
    ID3D11DeviceContext1* context_; ID3DDeviceContextState* previous_;
    StateScope(const StateScope&); StateScope& operator=(const StateScope&);
public:
    StateScope(ID3D11DeviceContext1* c, ID3DDeviceContextState* owned) : context_(c), previous_(NULL) { context_->SwapDeviceContextState(owned, &previous_); }
    ~StateScope() { if(previous_) { context_->SwapDeviceContextState(previous_, NULL); previous_->Release(); } }
    bool valid() const { return previous_ != NULL; }
};

struct BlitConstants { float src[4]; float srcSize[2]; float dstOrigin[2]; float dstSize[2]; uint32_t flags, pad; };

class Blitter {
    ComPtr<ID3D11Device> device_; ComPtr<ID3DDeviceContextState> state_;
    ComPtr<ID3D11VertexShader> vs_; ComPtr<ID3D11PixelShader> ps_; ComPtr<ID3D11Buffer> constants_;
    ComPtr<ID3D11SamplerState> sampler_; ComPtr<ID3D11RasterizerState> raster_; ComPtr<ID3D11DepthStencilState> depth_;
    struct Target { ID3D11Texture2D* texture; ComPtr<ID3D11RenderTargetView> view; bool linearize; };
    std::vector<Target> targets_;              // swapchain images live for the session; bounded cache
    struct Scratch { ComPtr<ID3D11Texture2D> texture; D3D11_TEXTURE2D_DESC desc; };
    std::vector<Scratch> scratch_;             // copies of sources without a shader-resource binding
    Target& target(ID3D11Texture2D* t)
    {
        for(auto& x : targets_) if(x.texture == t) return x;
        D3D11_TEXTURE2D_DESC d{}; t->GetDesc(&d);
        Target x{t, nullptr, false};
        D3D11_RENDER_TARGET_VIEW_DESC rd{}; rd.Format = TargetView(d.Format, x.linearize); rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        Need(rd.Format != DXGI_FORMAT_UNKNOWN && (d.BindFlags & D3D11_BIND_RENDER_TARGET) && d.SampleDesc.Count == 1, "blit_target_format");
        Hr(device_->CreateRenderTargetView(t, &rd, &x.view), "blit_target_view");
        if(targets_.size() >= 96) targets_.clear();
        targets_.push_back(x); return targets_.back();
    }
public:
    bool ready() const { return ps_ != nullptr; }
    void close() { targets_.clear(); scratch_.clear(); depth_.Reset(); raster_.Reset(); sampler_.Reset(); constants_.Reset(); ps_.Reset(); vs_.Reset(); state_.Reset(); device_.Reset(); }
    void init(ID3D11Device* d)
    {
        if(ready()) return;
        device_ = d; ComPtr<ID3D11Device1> d1; Hr(d->QueryInterface(IID_PPV_ARGS(&d1)), "blit_device1");
        const D3D_FEATURE_LEVEL level = d->GetFeatureLevel();
        Hr(d1->CreateDeviceContextState(0, &level, 1, D3D11_SDK_VERSION, __uuidof(ID3D11Device1), NULL, &state_), "blit_state");
        Hr(d->CreateVertexShader(GxChainVs, sizeof(GxChainVs), NULL, &vs_), "blit_vs");
        D3D11_BUFFER_DESC b{}; b.ByteWidth = sizeof(BlitConstants); b.Usage = D3D11_USAGE_DEFAULT; b.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        Hr(d->CreateBuffer(&b, NULL, &constants_), "blit_constants");
        D3D11_SAMPLER_DESC s{}; s.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; s.MaxLOD = D3D11_FLOAT32_MAX;
        Hr(d->CreateSamplerState(&s, &sampler_), "blit_sampler");
        D3D11_RASTERIZER_DESC r{}; r.FillMode = D3D11_FILL_SOLID; r.CullMode = D3D11_CULL_NONE; r.DepthClipEnable = TRUE;
        Hr(d->CreateRasterizerState(&r, &raster_), "blit_raster");
        D3D11_DEPTH_STENCIL_DESC z{}; z.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; Hr(d->CreateDepthStencilState(&z, &depth_), "blit_depth");
        Hr(d->CreatePixelShader(GxChainPs, sizeof(GxChainPs), NULL, &ps_), "blit_ps");
    }
    // source rect (sx,sy,sw,sh of a srcW x srcH texture) -> target viewport (dx,dy,dw,dh); bilinear unless sizes match
    void draw(ID3D11DeviceContext* c, ID3D11Texture2D* t, uint32_t dx, uint32_t dy, uint32_t dw, uint32_t dh, ID3D11ShaderResourceView* src,
              uint32_t sx, uint32_t sy, uint32_t sw, uint32_t sh, uint32_t srcW, uint32_t srcH, bool opaque)
    {
        Need(ready() && c && t && src && dw && dh && sw && sh, "blit_input");
        Target& x = target(t);
        BlitConstants k{{(float)sx, (float)sy, (float)sw, (float)sh}, {(float)srcW, (float)srcH}, {(float)dx, (float)dy}, {(float)dw, (float)dh},
                        (sw != dw || sh != dh ? 1u : 0u) | (x.linearize ? 2u : 0u) | (opaque ? 4u : 0u), 0};
        ComPtr<ID3D11DeviceContext1> c1; Hr(c->QueryInterface(IID_PPV_ARGS(&c1)), "blit_context1");
        StateScope saved(c1.Get(), state_.Get()); Need(saved.valid(), "blit_state_swap");
        c1->ClearState();
        c1->UpdateSubresource(constants_.Get(), 0, NULL, &k, 0, 0);
        D3D11_VIEWPORT v{}; v.TopLeftX = (float)dx; v.TopLeftY = (float)dy; v.Width = (float)dw; v.Height = (float)dh; v.MaxDepth = 1;
        c1->RSSetViewports(1, &v); c1->RSSetState(raster_.Get());
        ID3D11RenderTargetView* rtv = x.view.Get(); c1->OMSetRenderTargets(1, &rtv, NULL); c1->OMSetDepthStencilState(depth_.Get(), 0);
        c1->IASetInputLayout(NULL); c1->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        c1->VSSetShader(vs_.Get(), NULL, 0); c1->PSSetShader(ps_.Get(), NULL, 0);
        ID3D11Buffer* cb = constants_.Get(); c1->PSSetConstantBuffers(0, 1, &cb);
        ID3D11SamplerState* sm = sampler_.Get(); c1->PSSetSamplers(0, 1, &sm); c1->PSSetShaderResources(0, 1, &src);
        c1->Draw(3, 0);
        c1->ClearState();   // drop our references to the game's / swapchain resources from the owned state
    }
    // a shader view of a single-sample 8-bit / RGBA16F source; one without the shader binding goes through a cached scratch copy
    ComPtr<ID3D11ShaderResourceView> readable(ID3D11DeviceContext* c, ID3D11Texture2D* s)
    {
        D3D11_TEXTURE2D_DESC sd{}; s->GetDesc(&sd);
        Need(ready() && sd.ArraySize == 1 && sd.SampleDesc.Count == 1, "copy_source_shape");
        ID3D11Texture2D* readable = s;
        if(!(sd.BindFlags & D3D11_BIND_SHADER_RESOURCE)) {
            Scratch* hit = nullptr;
            for(auto& x : scratch_) if(x.desc.Width == sd.Width && x.desc.Height == sd.Height && x.desc.Format == sd.Format) hit = &x;
            if(!hit) {
                if(scratch_.size() >= 8) scratch_.erase(scratch_.begin());
                Scratch n; n.desc = sd; n.desc.BindFlags = D3D11_BIND_SHADER_RESOURCE; n.desc.MiscFlags = 0; n.desc.CPUAccessFlags = 0; n.desc.Usage = D3D11_USAGE_DEFAULT;
                n.desc.MipLevels = 1; Hr(device_->CreateTexture2D(&n.desc, NULL, &n.texture), "copy_scratch");
                scratch_.push_back(n); hit = &scratch_.back();
            }
            c->CopySubresourceRegion(hit->texture.Get(), 0, 0, 0, 0, s, 0, NULL); readable = hit->texture.Get();
        }
        D3D11_SHADER_RESOURCE_VIEW_DESC v{}; v.Format = SourceView(sd.Format); v.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; v.Texture2D.MipLevels = 1;
        Need(v.Format != DXGI_FORMAT_UNKNOWN, "copy_source_format");
        ComPtr<ID3D11ShaderResourceView> view; Hr(device_->CreateShaderResourceView(readable, &v, &view), "copy_source_view");
        return view;
    }
    // CopyResource / CopySubresourceRegion semantics into a target of another 8-bit family (RGBA swapchains, A2)
    void copy(ID3D11DeviceContext* c, ID3D11Texture2D* t, uint32_t dx, uint32_t dy, ID3D11Texture2D* s, const D3D11_BOX* box)
    {
        D3D11_TEXTURE2D_DESC sd{}; s->GetDesc(&sd);
        const uint32_t sx = box ? box->left : 0, sy = box ? box->top : 0, sw = box ? box->right - box->left : sd.Width, sh = box ? box->bottom - box->top : sd.Height;
        Need(sx + sw <= sd.Width && sy + sh <= sd.Height, "copy_source_rect");
        ComPtr<ID3D11ShaderResourceView> view = readable(c, s);
        draw(c, t, dx, dy, sw, sh, view.Get(), sx, sy, sw, sh, sd.Width, sd.Height, false);
    }
};

// One forwarder copy (nvngx.dll_gtavxr.dll = NR feature 18, nvngx.dll_gtavxr_sr.dll = SR feature 1): every NGX call is made
// from inside it (the models accept callers whose path contains "nvngx.dll"; P2).
struct Forwarder {
    HMODULE m = nullptr; bool ready = false;
    GxFwdLoadFn load = nullptr; GxFwdInitFn init = nullptr; GxFwdAllocFn alloc = nullptr; GxFwdDestroyFn destroy = nullptr;
    GxFwdSetU32Fn setU = nullptr; GxFwdSetF32Fn setF = nullptr; GxFwdSetResFn setR = nullptr; GxFwdCreateFn create = nullptr;
    GxFwdEvaluateFn evaluate = nullptr; GxFwdReleaseFn release = nullptr; GxFwdShutdownFn shutdown = nullptr; void (*feature)(unsigned) = nullptr;
    template<class F> bool get(F& f, const char* n) { f = reinterpret_cast<F>(GetProcAddress(m, n)); return f != nullptr; }
    // 0 = ready; otherwise the failing step (1 forwarder, 2 exports, 3 model load, 4 model exports)
    int open(const std::wstring& forwarder, const std::wstring& model, unsigned featureId)
    {
        m = LoadLibraryExW(forwarder.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        if(!m) return 1;
        if(!get(load, "gx_nr_load") || !get(init, "gx_nr_init") || !get(alloc, "gx_nr_alloc") || !get(destroy, "gx_nr_destroy") ||
           !get(setU, "gx_nr_set_u32") || !get(setF, "gx_nr_set_f32") || !get(setR, "gx_nr_set_res") || !get(create, "gx_nr_create") ||
           !get(evaluate, "gx_nr_evaluate") || !get(release, "gx_nr_release") || !get(shutdown, "gx_nr_shutdown") || !get(feature, "gx_nr_set_feature")) return 2;
        if(GetFileAttributesW(model.c_str()) == INVALID_FILE_ATTRIBUTES) return 3;
        feature(featureId);
        GxFwdExports ex{};
        if(load(model.c_str(), &ex) != 0) return 3;
        if(!ex.init || !ex.create || !ex.evaluate || !ex.release) return 4;
        ready = true; return 0;
    }
};
inline bool NgxGood(int r) { return NgxOk(r) && r != GX_FWD_EXCEPTION && r != GX_FWD_NO_EXPORT && r != GX_FWD_NOT_LOADED && r != GX_FWD_BAD_ARGUMENT; }

struct CaptureConstants { float relFromClip[16]; float prevClipFromRel[16]; float size[2]; float jitter[2]; uint32_t reset, pad[3]; };

class Chain {
public:
    // DLSS conventions, measured by the offline alignment control P4 against the real SR model (2026-11-03: jitter +1 / MV +1
    // error 0.0100, MV -1 0.0117, jitter -1 0.0172 / 0.0215 vs a 2x2 output-size ground truth of a moving camera)
    float jitterSign = 1.f;        // NGX Jitter.Offset = jitterSign * measured image shift
    float motionScale = 1.f;       // NGX MV.Scale (the capture writes previous - current, render pixels)
    std::function<void(const std::string&)> log = [](const std::string&) {};
    bool timing = false;           // developer mode readings (GPU ms per stage)

    bool active() const { return configured_; }
    uint32_t render_width() const { return rw_; }
    uint32_t render_height() const { return rh_; }

    // 0 = NR/SR ready, 1 = runs as the D3D11 bilinear blit (reason logged); throws only when even the blit cannot exist
    int configure(ID3D11Device* d, ID3D11DeviceContext* c, const GxEyeChainConfig& k, const std::wstring& dataDir, const std::wstring& bridgeDir)
    {
        Need(!configured_ && d && c && k.render_width && k.render_height && k.output_width && k.output_height &&
             k.render_width <= k.output_width && k.render_height <= k.output_height && k.upscale < gx_eye_chain::UPSCALE_COUNT && k.neural_intensity <= 100,
             "chain_config");
        device_ = d; context_ = c; rw_ = k.render_width; rh_ = k.render_height; ow_ = k.output_width; oh_ = k.output_height;
        upscale_ = k.upscale; nrWanted_ = k.neural != 0; intensity_ = k.neural_intensity; dataDir_ = dataDir;
        dlssDir_ = k.dlss_dir ? k.dlss_dir : L""; forwarderDir_ = k.forwarder_dir ? k.forwarder_dir : bridgeDir.c_str();
        blit_.init(d);
        configured_ = true;
        try { open(); } catch(const Error& e) { fail(e); return 1; }
        state_ = gx_eye_chain::CHAIN;
        say("EYE_CHAIN state=chain render=" + std::to_string(rw_) + "x" + std::to_string(rh_) + " output=" + std::to_string(ow_) + "x" + std::to_string(oh_) +
            " sr=" + std::to_string(srOn_ ? 1 : 0) + " nr=" + std::to_string(nrReady_ ? 1 : 0) + (nrNote_.empty() ? "" : " nr_note=" + nrNote_));
        return 0;
    }
    // the guides of the eye whose capture comes next (NULL = no fresh guides: that eye starts a new history) and the scene depth
    void guides(const GxEyeGuides* g, ID3D11Resource* depth)
    {
        pendingDepth_ = depth; pendingHas_ = g != nullptr;
        if(g) { Need(g->size == sizeof(*g) && g->version == 1 && g->eye < 2, "guides_abi"); pending_ = *g; }
        pendingSet_ = true;
    }
    void set_params(uint32_t enabled, uint32_t intensity, uint32_t compare)
    {
        Need(enabled <= 1 && intensity <= 100 && compare <= 1, "neural_params");
        if(enabled && !nrWanted_) for(auto& e : eyes_) e.nrHistory = false;
        nrWanted_ = enabled != 0; intensity_ = intensity; compare_ = compare;
    }
    void status(GxEyeChainStatus& s) const
    {
        memset(&s, 0, sizeof(s)); s.size = sizeof(s); s.version = 1; s.state = state_; s.neural_active = nrReady_ && nrWanted_ ? 1u : 0u;
        s.upscale = srOn_ ? upscale_ : 0u; s.compare = compare_; s.fault_stage = faultStage_; s.fault_code = (uint32_t)faultCode_;
        s.ms_capture = (float)ms_[0]; s.ms_neural = (float)ms_[1]; s.ms_upscale = (float)ms_[2]; s.ms_return = (float)ms_[3];
        strncpy_s(s.reason, sizeof(s.reason), reason_.c_str(), _TRUNCATE);
    }
    // world eye e: source = the game's render-size eye output, target = this eye's output-size swapchain image
    void present(ID3D11DeviceContext* c, unsigned e, uint64_t generation, ID3D11Texture2D* source, ID3D11Texture2D* target)
    {
        Need(configured_ && e < 2 && source && target, "present_input");
        const bool had = pendingSet_; pendingSet_ = false;
        ComPtr<ID3D11Resource> depth; depth.Swap(pendingDepth_);
        if(state_ == gx_eye_chain::CHAIN && had && depth) {
            try { run(c, e, generation, source, target, depth.Get()); return; } catch(const Error& x) { fail(x); }
        }
        eyes_[e].history = false; eyes_[e].nrHistory = false;
        plain(c, source, target);
    }
    void close()
    {
        if(queue_ && fence12_ && event_) { const UINT64 v = ++value_; if(SUCCEEDED(queue_->Signal(fence12_.Get(), v))) wait(v, 2000); }
        for(auto& e : eyes_) {
            if(e.srH && sr_.ready) sr_.release(e.srH); if(e.srP && sr_.ready) sr_.destroy(e.srP);
            if(e.nrH && nr_.ready) nr_.release(e.nrH); if(e.nrP && nr_.ready) nr_.destroy(e.nrP);
            e = Eye();
        }
        if(dev12_) { if(sr_.ready) sr_.shutdown(dev12_.Get()); if(nr_.ready) nr_.shutdown(dev12_.Get()); }
        sr_ = Forwarder(); nr_ = Forwarder();   // modules stay loaded until process exit (no unload under a live runtime)
        if(event_) CloseHandle(event_); event_ = NULL;
        for(auto& t : t11_) t = Times11();
        rb_.Reset(); qheap_.Reset(); for(auto& a : alloc_) a.Reset(); list_.Reset(); queue_.Reset(); fence11_.Reset(); fence12_.Reset(); dev12_.Reset();
        capture_.Reset(); captureCb_.Reset(); captureState_.Reset(); ctx4_.Reset(); dev5_.Reset(); pendingDepth_.Reset();
        blit_.close(); configured_ = false; state_ = gx_eye_chain::NATIVE;
    }

private:
    static const unsigned kSlots = 6;
    struct Eye {
        ComPtr<ID3D11Texture2D> color, depth, motion, out; ComPtr<ID3D11UnorderedAccessView> colorU, depthU, motionU; ComPtr<ID3D11ShaderResourceView> outS;
        ComPtr<ID3D12Resource> color12, depth12, motion12, out12, nr12;
        void* srP = nullptr; void* srH = nullptr; void* nrP = nullptr; void* nrH = nullptr;
        bool history = false, nrHistory = false; LONGLONG last = 0;
    };
    ComPtr<ID3D11Device> device_; ID3D11DeviceContext* context_ = nullptr; ComPtr<ID3D11Device5> dev5_; ComPtr<ID3D11DeviceContext4> ctx4_;
    ComPtr<ID3DDeviceContextState> captureState_; ComPtr<ID3D11ComputeShader> capture_; ComPtr<ID3D11Buffer> captureCb_;
    ComPtr<ID3D12Device> dev12_; ComPtr<ID3D12CommandQueue> queue_; ComPtr<ID3D12CommandAllocator> alloc_[kSlots]; ComPtr<ID3D12GraphicsCommandList> list_;
    ComPtr<ID3D12Fence> fence12_; ComPtr<ID3D11Fence> fence11_; HANDLE event_ = NULL; UINT64 value_ = 0, slotValue_[kSlots] = {}; unsigned submissions_ = 0;
    ComPtr<ID3D12QueryHeap> qheap_; ComPtr<ID3D12Resource> rb_; UINT64 freq_ = 0; bool slotTimed_[kSlots] = {};
    // D3D11 side per slot: disjoint + capture start/end + return start/end (read without waiting when the slot comes back)
    struct Times11 { ComPtr<ID3D11Query> disjoint, t[4]; bool pending = false; } t11_[kSlots];
    Forwarder sr_, nr_; Eye eyes_[2]; Blitter blit_;
    GxEyeGuides pending_{}; ComPtr<ID3D11Resource> pendingDepth_; bool pendingSet_ = false, pendingHas_ = false;
    bool configured_ = false, srOn_ = false, nrReady_ = false, nrWanted_ = false, nrTried_ = false;
    uint32_t rw_ = 0, rh_ = 0, ow_ = 0, oh_ = 0, upscale_ = 0, intensity_ = 50, compare_ = 0, state_ = gx_eye_chain::NATIVE, faultStage_ = 0;
    long faultCode_ = 0; std::string reason_, nrNote_; std::wstring dataDir_, dlssDir_, forwarderDir_; double ms_[4] = {};

    void say(const std::string& s) { try { log(s); } catch(...) {} }
    static std::string hex(long v) { char b[16]; sprintf_s(b, "0x%08lX", (unsigned long)v); return b; }
    void fail(const Error& e)
    {
        state_ = gx_eye_chain::BLIT_FALLBACK; reason_ = e.stage; faultCode_ = e.code; faultStage_ = 1;
        say("EYE_CHAIN state=blit_fallback stage=" + reason_ + " code=" + hex(e.code));
    }
    void plain(ID3D11DeviceContext* c, ID3D11Texture2D* source, ID3D11Texture2D* target)
    {
        D3D11_TEXTURE2D_DESC s{}, t{}; source->GetDesc(&s); target->GetDesc(&t);
        ComPtr<ID3D11ShaderResourceView> view = blit_.readable(c, source);
        blit_.draw(c, target, 0, 0, t.Width, t.Height, view.Get(), 0, 0, s.Width, s.Height, s.Width, s.Height, true);
    }
    bool wait(UINT64 v, DWORD ms)
    {
        if(fence12_->GetCompletedValue() >= v) return true;
        if(FAILED(fence12_->SetEventOnCompletion(v, event_))) return false;
        WaitForSingleObject(event_, ms); return fence12_->GetCompletedValue() >= v;
    }
    void submit_and_wait(const char* stage)
    {
        Hr(list_->Close(), stage); ID3D12CommandList* l[] = {list_.Get()}; queue_->ExecuteCommandLists(1, l);
        const UINT64 v = ++value_; Hr(queue_->Signal(fence12_.Get(), v), stage); Need(wait(v, 5000), stage); Hr(dev12_->GetDeviceRemovedReason(), stage);
        Hr(alloc_[0]->Reset(), stage); Hr(list_->Reset(alloc_[0].Get(), NULL), stage);
    }
    ComPtr<ID3D11Texture2D> shared(DXGI_FORMAT f, uint32_t w, uint32_t h, bool uav, ComPtr<ID3D12Resource>& opened, const char* stage)
    {
        D3D11_TEXTURE2D_DESC d{}; d.Width = w; d.Height = h; d.MipLevels = d.ArraySize = 1; d.Format = f; d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE | (uav ? D3D11_BIND_UNORDERED_ACCESS : 0); d.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;
        ComPtr<ID3D11Texture2D> t; Hr(device_->CreateTexture2D(&d, NULL, &t), stage);
        ComPtr<IDXGIResource1> r; Hr(t.As(&r), stage); HANDLE h11 = NULL;
        Hr(r->CreateSharedHandle(NULL, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, NULL, &h11), stage);
        const HRESULT hr = dev12_->OpenSharedHandle(h11, IID_PPV_ARGS(&opened)); CloseHandle(h11); Hr(hr, stage);
        return t;
    }
    void open()
    {
        Hr(device_.As(&dev5_), "d3d11_device5"); ComPtr<ID3D11DeviceContext> c(context_); Hr(c.As(&ctx4_), "d3d11_context4");
        ComPtr<ID3D11Device1> d1; Hr(device_.As(&d1), "d3d11_device1"); const D3D_FEATURE_LEVEL level = device_->GetFeatureLevel();
        Hr(d1->CreateDeviceContextState(0, &level, 1, D3D11_SDK_VERSION, __uuidof(ID3D11Device1), NULL, &captureState_), "capture_state");
        Hr(device_->CreateComputeShader(GxChainCs, sizeof(GxChainCs), NULL, &capture_), "capture_cs");
        D3D11_BUFFER_DESC b{}; b.ByteWidth = sizeof(CaptureConstants); b.Usage = D3D11_USAGE_DEFAULT; b.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        Hr(device_->CreateBuffer(&b, NULL, &captureCb_), "capture_constants");
        // D3D12 on the game's adapter, loaded only here (the native path never touches d3d12.dll)
        HMODULE d3d12 = LoadLibraryW(L"d3d12.dll"); Need(d3d12 != NULL, "d3d12_dll");
        auto create12 = reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(GetProcAddress(d3d12, "D3D12CreateDevice")); Need(create12 != nullptr, "d3d12_entry");
        ComPtr<IDXGIDevice> dx; ComPtr<IDXGIAdapter> adapter; Hr(device_.As(&dx), "dxgi_device"); Hr(dx->GetAdapter(&adapter), "dxgi_adapter");
        Hr(create12(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev12_)), "d3d12_device");
        D3D12_COMMAND_QUEUE_DESC qd{}; qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; Hr(dev12_->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue_)), "d3d12_queue");
        for(auto& a : alloc_) Hr(dev12_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&a)), "d3d12_allocator");
        Hr(dev12_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc_[0].Get(), NULL, IID_PPV_ARGS(&list_)), "d3d12_list");
        Hr(dev12_->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&fence12_)), "shared_fence");
        { HANDLE h = NULL; Hr(dev12_->CreateSharedHandle(fence12_.Get(), NULL, GENERIC_ALL, NULL, &h), "shared_fence_handle");
          const HRESULT hr = dev5_->OpenSharedFence(h, IID_PPV_ARGS(&fence11_)); CloseHandle(h); Hr(hr, "shared_fence_open"); }
        event_ = CreateEventW(NULL, FALSE, FALSE, NULL); Need(event_ != NULL, "fence_event");
        D3D12_QUERY_HEAP_DESC qh{}; qh.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP; qh.Count = kSlots * 3; Hr(dev12_->CreateQueryHeap(&qh, IID_PPV_ARGS(&qheap_)), "query_heap");
        D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_READBACK; D3D12_RESOURCE_DESC rd{}; rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = kSlots * 3 * 8; rd.Height = rd.DepthOrArraySize = rd.MipLevels = 1; rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        Hr(dev12_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&rb_)), "query_readback");
        Hr(queue_->GetTimestampFrequency(&freq_), "timestamp_frequency");
        // D3D11 timestamps of the capture and return stages (developer readings): a missing query only leaves those ms at 0
        for(auto& t : t11_) {
            D3D11_QUERY_DESC qd11{}; qd11.Query = D3D11_QUERY_TIMESTAMP_DISJOINT; bool ok = SUCCEEDED(device_->CreateQuery(&qd11, &t.disjoint));
            qd11.Query = D3D11_QUERY_TIMESTAMP; for(auto& q : t.t) ok = ok && SUCCEEDED(device_->CreateQuery(&qd11, &q));
            if(!ok) { t.disjoint.Reset(); for(auto& q : t.t) q.Reset(); }
        }
        // per-eye textures: inputs at render size; the output at swapchain size
        for(auto& e : eyes_) {
            e.color = shared(DXGI_FORMAT_R16G16B16A16_FLOAT, rw_, rh_, true, e.color12, "eye_color");
            e.depth = shared(DXGI_FORMAT_R32_FLOAT, rw_, rh_, true, e.depth12, "eye_depth");
            e.motion = shared(DXGI_FORMAT_R16G16_FLOAT, rw_, rh_, true, e.motion12, "eye_motion");
            e.out = shared(DXGI_FORMAT_R16G16B16A16_FLOAT, ow_, oh_, true, e.out12, "eye_output");
            Hr(device_->CreateUnorderedAccessView(e.color.Get(), NULL, &e.colorU), "eye_color_uav");
            Hr(device_->CreateUnorderedAccessView(e.depth.Get(), NULL, &e.depthU), "eye_depth_uav");
            Hr(device_->CreateUnorderedAccessView(e.motion.Get(), NULL, &e.motionU), "eye_motion_uav");
            Hr(device_->CreateShaderResourceView(e.out.Get(), NULL, &e.outS), "eye_output_srv");
            D3D12_HEAP_PROPERTIES dh{}; dh.Type = D3D12_HEAP_TYPE_DEFAULT; D3D12_RESOURCE_DESC td{}; td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            td.Width = rw_; td.Height = rh_; td.DepthOrArraySize = td.MipLevels = 1; td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; td.SampleDesc.Count = 1;
            td.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            Hr(dev12_->CreateCommittedResource(&dh, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, NULL, IID_PPV_ARGS(&e.nr12)), "eye_nr_output");
        }
        // NGX: SR is required when an upscale is asked; NR is optional (missing model = NR unavailable, SR keeps running)
        Need(!dlssDir_.empty(), "dlss_dir");
        if(upscale_) {
            const int r = sr_.open(forwarderDir_ + L"\\nvngx.dll_gtavxr_sr.dll", dlssDir_ + L"\\nvngx_dlss.dll", 1);
            if(r) throw Error{r == 1 || r == 2 ? "sr_forwarder" : "sr_model", r};
            int n = sr_.init(0x47544158ull, dataDir_.c_str(), dev12_.Get()); if(!NgxGood(n)) throw Error{"sr_init", n};
            for(auto& e : eyes_) {
                n = sr_.alloc(&e.srP); if(!NgxGood(n) || !e.srP) throw Error{"sr_alloc", n};
                sr_.setU(e.srP, "Width", rw_); sr_.setU(e.srP, "Height", rh_); sr_.setU(e.srP, "OutWidth", ow_); sr_.setU(e.srP, "OutHeight", oh_);
                sr_.setU(e.srP, "PerfQualityValue", 0); sr_.setU(e.srP, "DLSS.Feature.Create.Flags", (1u << 1) | (1u << 3) | (1u << 6));  // MV low-res, depth inverted, auto exposure
                sr_.setU(e.srP, "CreationNodeMask", 1); sr_.setU(e.srP, "VisibilityNodeMask", 1);
                n = sr_.create(list_.Get(), e.srP, &e.srH); if(!NgxGood(n) || !e.srH) throw Error{"sr_create", n};
                submit_and_wait("sr_create_submit");
            }
            srOn_ = true;
        }
        if(nrWanted_) open_nr();
        Hr(list_->Close(), "setup_close");
        Need(srOn_ || nrReady_, nrNote_.empty() ? "nothing_to_run" : "nr_model");
    }
    // NR switched on from the VR menu in a session that started without it: one idle wait, then the features are created
    void open_nr_late()
    {
        Need(wait(value_, 2000), "late_nr_idle");
        Hr(alloc_[0]->Reset(), "late_nr_reset"); Hr(list_->Reset(alloc_[0].Get(), NULL), "late_nr_list");
        open_nr(); Hr(list_->Close(), "late_nr_close");
        say(std::string("EYE_CHAIN nr_late=") + (nrReady_ ? "ready" : nrNote_));
    }
    void open_nr()
    {
        nrTried_ = true;
        const int r = nr_.open(forwarderDir_ + L"\\nvngx.dll_gtavxr.dll", dlssDir_ + L"\\nvngx_dlssnr.dll", kNgxNeuralRenderingFeature);
        if(r) { nrNote_ = r == 1 || r == 2 ? "nr_forwarder" : "nr_model_missing"; return; }
        int n = nr_.init(0x47544158ull, dataDir_.c_str(), dev12_.Get()); if(!NgxGood(n)) { nrNote_ = "nr_init " + hex(n); return; }
        for(auto& e : eyes_) {
            n = nr_.alloc(&e.nrP); if(!NgxGood(n) || !e.nrP) { nrNote_ = "nr_alloc " + hex(n); return; }
            static const char* sizes[] = {"Width", "Height", "OutWidth", "OutHeight", "DLSSNR.Width", "DLSSNR.Height", "DLSSNR.InputWidth", "DLSSNR.InputHeight",
                                          "DLSSNR.OutputWidth", "DLSSNR.OutputHeight", "DLSSNR.Output.Width", "DLSSNR.Output.Height"};
            for(int i = 0; i < 12; ++i) nr_.setU(e.nrP, sizes[i], (i & 1) ? rh_ : rw_);
            nr_.setF(e.nrP, "DLSSNR.ScalingRatio", 1.0f); nr_.setF(e.nrP, "DLSSNR.Scale", 1.0f); nr_.setU(e.nrP, "DLSSNR.Upscaling", 0); nr_.setU(e.nrP, "DLSSNR.Enabled", 1);
            nr_.setU(e.nrP, "DLSSNR.Hint.Render.Preset", 0); nr_.setU(e.nrP, "DLSSNR.Style", 0); nr_.setF(e.nrP, "DLSSNR.Intensity", intensity_ / 100.f);
            nr_.setF(e.nrP, "DLSSNR.LocalToneStrength", 1.0f); nr_.setF(e.nrP, "DLSSNR.LocalStructureStrength", 1.0f); nr_.setF(e.nrP, "DLSSNR.GlobalToneStrength", 1.0f);
            nr_.setF(e.nrP, "DLSSNR.SkinStructureStrength", 1.0f); nr_.setU(e.nrP, "DLSSNR.UseAutoMask", 0); nr_.setU(e.nrP, "DLSSNR.UICorrection", 0);
            nr_.setU(e.nrP, "PerfQualityValue", 0); nr_.setU(e.nrP, "DLSS.Feature.Create.Flags", 0); nr_.setU(e.nrP, "CreationNodeMask", 1); nr_.setU(e.nrP, "VisibilityNodeMask", 1);
            n = nr_.create(list_.Get(), e.nrP, &e.nrH); if(!NgxGood(n) || !e.nrH) { nrNote_ = "nr_create " + hex(n); return; }
            submit_and_wait("nr_create_submit");
        }
        nrReady_ = true;
    }
    static D3D12_RESOURCE_BARRIER transition(ID3D12Resource* r, D3D12_RESOURCE_STATES a, D3D12_RESOURCE_STATES b)
    {
        D3D12_RESOURCE_BARRIER x{}; x.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; x.Transition.pResource = r;
        x.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; x.Transition.StateBefore = a; x.Transition.StateAfter = b; return x;
    }
    static D3D12_RESOURCE_BARRIER uav(ID3D12Resource* r) { D3D12_RESOURCE_BARRIER x{}; x.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV; x.UAV.pResource = r; return x; }
    void average(unsigned k, double ms) { ms_[k] = ms_[k] ? ms_[k] + (ms - ms_[k]) / 120.0 : ms; }

    void run(ID3D11DeviceContext* c, unsigned eyeIndex, uint64_t generation, ID3D11Texture2D* source, ID3D11Texture2D* target, ID3D11Resource* depth)
    {
        Eye& e = eyes_[eyeIndex];
        if(nrWanted_ && !nrTried_) open_nr_late();
        // inputs: the game's eye colour and scene depth, render size, single sample
        D3D11_TEXTURE2D_DESC s{}; source->GetDesc(&s);
        Need(s.Width == rw_ && s.Height == rh_ && FamilyOf(s.Format) == BGRA8, "source_shape");
        ComPtr<ID3D11Texture2D> depthTexture; Hr(depth->QueryInterface(IID_PPV_ARGS(&depthTexture)), "depth_texture");
        D3D11_TEXTURE2D_DESC z{}; depthTexture->GetDesc(&z);
        Need(z.Width == rw_ && z.Height == rh_ && z.SampleDesc.Count == 1 && (z.BindFlags & D3D11_BIND_SHADER_RESOURCE) && DepthView(z.Format) != DXGI_FORMAT_UNKNOWN, "depth_format");
        D3D11_SHADER_RESOURCE_VIEW_DESC dv{}; dv.Format = DepthView(z.Format); dv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; dv.Texture2D.MipLevels = 1;
        ComPtr<ID3D11ShaderResourceView> colorS = blit_.readable(c, source), depthS;
        Hr(device_->CreateShaderResourceView(depthTexture.Get(), &dv, &depthS), "depth_srv");
        // history: fresh guides of this eye and pair, and no reset
        const bool fresh = pendingHas_ && pending_.eye == eyeIndex && pending_.generation == generation && pending_.render_width == rw_ && pending_.render_height == rh_;
        const bool reset = !fresh || pending_.reset || !e.history;
        CaptureConstants k{};
        if(fresh) { memcpy(k.relFromClip, pending_.rel_from_clip, 64); memcpy(k.prevClipFromRel, pending_.prev_clip_from_rel, 64); k.jitter[0] = pending_.jitter_px[0]; k.jitter[1] = pending_.jitter_px[1]; }
        k.size[0] = (float)rw_; k.size[1] = (float)rh_; k.reset = reset ? 1u : 0u;
        LARGE_INTEGER now; QueryPerformanceCounter(&now); LARGE_INTEGER f; QueryPerformanceFrequency(&f);
        const float dtMs = e.last ? (float)(1000.0 * (now.QuadPart - e.last) / f.QuadPart) : 11.1f; e.last = now.QuadPart;
        const unsigned slot = submissions_++ % kSlots;
        // D3D11 capture / return ms (developer readings): this slot's previous result if the GPU already has it, never a wait
        Times11& t11 = t11_[slot];
        if(t11.pending) {
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj{}; UINT64 ts[4] = {}; bool ready = c->GetData(t11.disjoint.Get(), &dj, sizeof(dj), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK;
            for(unsigned i = 0; ready && i < 4; ++i) ready = c->GetData(t11.t[i].Get(), &ts[i], sizeof(ts[i]), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK;
            if(ready && !dj.Disjoint && dj.Frequency && ts[1] >= ts[0] && ts[3] >= ts[2]) {
                average(0, 1000.0 * (ts[1] - ts[0]) / dj.Frequency); average(3, 1000.0 * (ts[3] - ts[2]) / dj.Frequency);
            }
            t11.pending = false;
        }
        const bool time11 = timing && t11.disjoint;
        // the disjoint query is always closed, also when this eye fails half-way (the next Begin then starts clean)
        struct DisjointEnd { ID3D11DeviceContext* c; ID3D11Query* q; ~DisjointEnd() { if(q) c->End(q); } } disjointEnd{c, nullptr};
        if(time11) { c->Begin(t11.disjoint.Get()); disjointEnd.q = t11.disjoint.Get(); c->End(t11.t[0].Get()); }

        // 1. D3D11 capture into the shared inputs (owned context state), then signal
        {
            ComPtr<ID3D11DeviceContext1> c1; Hr(c->QueryInterface(IID_PPV_ARGS(&c1)), "capture_context1");
            StateScope saved(c1.Get(), captureState_.Get()); Need(saved.valid(), "capture_state_swap");
            c1->ClearState(); c1->UpdateSubresource(captureCb_.Get(), 0, NULL, &k, 0, 0);
            ID3D11ShaderResourceView* srv[2] = {colorS.Get(), depthS.Get()}; ID3D11UnorderedAccessView* uavs[3] = {e.colorU.Get(), e.depthU.Get(), e.motionU.Get()};
            ID3D11Buffer* cb = captureCb_.Get();
            c1->CSSetShader(capture_.Get(), NULL, 0); c1->CSSetConstantBuffers(0, 1, &cb); c1->CSSetShaderResources(0, 2, srv); c1->CSSetUnorderedAccessViews(0, 3, uavs, NULL);
            c1->Dispatch((rw_ + 7) / 8, (rh_ + 7) / 8, 1);
            c1->ClearState();
        }
        if(time11) c->End(t11.t[1].Get());
        const UINT64 inputs = ++value_; Hr(ctx4_->Signal(fence11_.Get(), inputs), "capture_signal"); c->Flush();

        // 2. D3D12: NR (render size) -> SR (render -> output) into the shared output
        Need(wait(slotValue_[slot], 250), "allocator_wait");
        if(timing && slotTimed_[slot]) {
            UINT64* q = nullptr; D3D12_RANGE r{slot * 24u, slot * 24u + 24u};
            if(SUCCEEDED(rb_->Map(0, &r, reinterpret_cast<void**>(&q)))) {
                const UINT64* t = q + slot * 3; average(1, 1000.0 * (t[1] - t[0]) / freq_); average(2, 1000.0 * (t[2] - t[1]) / freq_);
                D3D12_RANGE none{0, 0}; rb_->Unmap(0, &none);
            }
        }
        Hr(alloc_[slot]->Reset(), "allocator_reset"); Hr(list_->Reset(alloc_[slot].Get(), NULL), "list_reset");
        const bool nrRun = nrReady_ && nrWanted_ && e.nrH;
        {
            D3D12_RESOURCE_BARRIER b[4] = {transition(e.color12.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                                           transition(e.depth12.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                                           transition(e.motion12.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                                           transition(e.out12.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)};
            list_->ResourceBarrier(4, b);
        }
        list_->EndQuery(qheap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 3 + 0);
        ID3D12Resource* srInput = e.color12.Get();
        if(nrRun) {
            ID3D12Resource* nrOut = srOn_ ? e.nr12.Get() : e.out12.Get();
            nr_.setR(e.nrP, "DLSSNR.Color", e.color12.Get()); nr_.setR(e.nrP, "DLSSNR.Depth", e.depth12.Get()); nr_.setR(e.nrP, "DLSSNR.MVec", e.motion12.Get());
            nr_.setR(e.nrP, "DLSSNR.Output", nrOut); nr_.setU(e.nrP, "DLSSNR.Enabled", 1); nr_.setU(e.nrP, "DLSSNR.Width", rw_); nr_.setU(e.nrP, "DLSSNR.Height", rh_);
            nr_.setU(e.nrP, "DLSSNR.DepthInverted", 1); nr_.setU(e.nrP, "DLSSNR.Reset", reset || !e.nrHistory ? 1 : 0); nr_.setF(e.nrP, "DLSSNR.Intensity", intensity_ / 100.f);
            static const char* rects[] = {"Color", "Output", "Depth", "MVec"};
            for(const char* r : rects) { char n[64];
                sprintf_s(n, "DLSSNR.%sSubrectBaseX", r); nr_.setU(e.nrP, n, 0); sprintf_s(n, "DLSSNR.%sSubrectBaseY", r); nr_.setU(e.nrP, n, 0);
                sprintf_s(n, "DLSSNR.%sSubrectWidth", r); nr_.setU(e.nrP, n, rw_); sprintf_s(n, "DLSSNR.%sSubrectHeight", r); nr_.setU(e.nrP, n, rh_); }
            nr_.setF(e.nrP, "DLSSNR.MVecScaleX", 1.0f); nr_.setF(e.nrP, "DLSSNR.MVecScaleY", 1.0f);
            const int r = nr_.evaluate(list_.Get(), e.nrH, e.nrP); if(!NgxGood(r)) throw Error{"nr_evaluate", r};
            D3D12_RESOURCE_BARRIER u = uav(nrOut); list_->ResourceBarrier(1, &u);
            if(compare_) {   // A/B: the right half without NR (SR then upscales both halves with the same history)
                D3D12_RESOURCE_BARRIER b[2] = {transition(nrOut, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST),
                                               transition(e.color12.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE)};
                list_->ResourceBarrier(2, b);
                D3D12_TEXTURE_COPY_LOCATION dst{}, src{}; dst.pResource = nrOut; src.pResource = e.color12.Get();
                dst.Type = src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; D3D12_BOX box{rw_ / 2, 0, 0, rw_, rh_, 1};
                list_->CopyTextureRegion(&dst, rw_ / 2, 0, 0, &src, &box);
                for(auto& x : b) { const auto t = x.Transition.StateBefore; x.Transition.StateBefore = x.Transition.StateAfter; x.Transition.StateAfter = t; }
                list_->ResourceBarrier(2, b);
            }
            if(srOn_) { D3D12_RESOURCE_BARRIER b = transition(e.nr12.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE); list_->ResourceBarrier(1, &b); }
            srInput = e.nr12.Get();
        }
        list_->EndQuery(qheap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 3 + 1);
        if(srOn_) {
            sr_.setR(e.srP, "Color", srInput); sr_.setR(e.srP, "Output", e.out12.Get()); sr_.setR(e.srP, "Depth", e.depth12.Get()); sr_.setR(e.srP, "MotionVectors", e.motion12.Get());
            const float jx = fresh ? jitterSign * pending_.jitter_px[0] : 0.f, jy = fresh ? jitterSign * pending_.jitter_px[1] : 0.f;
            sr_.setF(e.srP, "Jitter.Offset.X", jx); sr_.setF(e.srP, "Jitter.Offset.Y", jy); sr_.setF(e.srP, "MV.Scale.X", motionScale); sr_.setF(e.srP, "MV.Scale.Y", motionScale);
            sr_.setU(e.srP, "Reset", reset ? 1 : 0); sr_.setU(e.srP, "DLSS.Render.Subrect.Dimensions.Width", rw_); sr_.setU(e.srP, "DLSS.Render.Subrect.Dimensions.Height", rh_);
            sr_.setF(e.srP, "FrameTimeDeltaInMsec", dtMs); sr_.setF(e.srP, "Sharpness", 0.0f);
            const int r = sr_.evaluate(list_.Get(), e.srH, e.srP); if(!NgxGood(r)) throw Error{"sr_evaluate", r};
            D3D12_RESOURCE_BARRIER u = uav(e.out12.Get()); list_->ResourceBarrier(1, &u);
            if(nrRun) { D3D12_RESOURCE_BARRIER b = transition(e.nr12.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS); list_->ResourceBarrier(1, &b); }
        } else if(!nrRun) {   // NR-only chain with NR switched off in the menu: the input goes straight to the output (same size)
            D3D12_RESOURCE_BARRIER b[2] = {transition(e.out12.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST),
                                           transition(e.color12.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE)};
            list_->ResourceBarrier(2, b); list_->CopyResource(e.out12.Get(), e.color12.Get());
            for(auto& x : b) { const auto t = x.Transition.StateBefore; x.Transition.StateBefore = x.Transition.StateAfter; x.Transition.StateAfter = t; }
            list_->ResourceBarrier(2, b);
        }
        list_->EndQuery(qheap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 3 + 2);
        {
            D3D12_RESOURCE_BARRIER b[4] = {transition(e.color12.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON),
                                           transition(e.depth12.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON),
                                           transition(e.motion12.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON),
                                           transition(e.out12.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON)};
            list_->ResourceBarrier(4, b);
        }
        list_->ResolveQueryData(qheap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 3, 3, rb_.Get(), slot * 24);
        Hr(list_->Close(), "list_close");
        // GPU-side waits only; the D3D11 wait below is issued after this submission was accepted (a removed D3D12 device
        // completes its fences, so the game's queue is never left waiting on a value nobody will signal)
        Hr(queue_->Wait(fence12_.Get(), inputs), "queue_wait");
        ID3D12CommandList* l[] = {list_.Get()}; queue_->ExecuteCommandLists(1, l);
        const UINT64 done = ++value_; Hr(queue_->Signal(fence12_.Get(), done), "queue_signal"); slotValue_[slot] = done; slotTimed_[slot] = timing;
        Hr(dev12_->GetDeviceRemovedReason(), "d3d12_removed");
        e.history = true; if(nrRun) e.nrHistory = true;

        // 3. D3D11: wait on the GPU, draw the output into the swapchain image (exact when sizes match, else bilinear)
        Hr(ctx4_->Wait(fence11_.Get(), done), "return_wait");
        if(time11) c->End(t11.t[2].Get());
        D3D11_TEXTURE2D_DESC t{}; target->GetDesc(&t);
        blit_.draw(c, target, 0, 0, t.Width, t.Height, e.outS.Get(), 0, 0, ow_, oh_, ow_, oh_, true);
        if(time11) { c->End(t11.t[3].Get()); c->End(t11.disjoint.Get()); disjointEnd.q = nullptr; t11.pending = true; }
    }
};

}
