#pragma once
// Stereo theatre presentation on D3D11. GTAVXR-owned. It keeps one private image per eye (the cutscene
// frame the game rendered for that eye) and draws it as a screen into the ordinary eye swapchain image.
// No XR swapchain is created or destroyed here: the frame stays a normal stereo projection frame.
#include "eye_chain_gpu.h"
#include "theatre.h"
#include "theatre_vs_bytes.h"
#include "theatre_ps_bytes.h"

namespace gx_theatre {
using Microsoft::WRL::ComPtr;
using gx_chain::Hr;
using gx_chain::Need;

struct ScreenConstants { float corner[4][4]; float uv[4]; uint32_t flags, pad[3]; };
static_assert(sizeof(ScreenConstants) == 96, "theatre constant buffer layout");

class Screen {
    ComPtr<ID3D11Device> device_; ComPtr<ID3DDeviceContextState> state_;
    ComPtr<ID3D11VertexShader> vs_; ComPtr<ID3D11PixelShader> ps_; ComPtr<ID3D11Buffer> constants_;
    ComPtr<ID3D11SamplerState> sampler_; ComPtr<ID3D11RasterizerState> raster_; ComPtr<ID3D11DepthStencilState> depth_;
    struct Image { ComPtr<ID3D11Texture2D> texture; ComPtr<ID3D11ShaderResourceView> view; };
    Image image_[2]; uint32_t width_ = 0, height_ = 0; DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN; bool mips_ = false;
    struct Target { ID3D11Texture2D* texture; ComPtr<ID3D11RenderTargetView> view; bool srgb; };
    std::vector<Target> targets_;  // swapchain images live for the session; bounded cache
    Target& target(ID3D11Texture2D* t) {
        for (auto& x : targets_) if (x.texture == t) return x;
        D3D11_TEXTURE2D_DESC d{}; t->GetDesc(&d);
        Target x{t, nullptr, false};
        D3D11_RENDER_TARGET_VIEW_DESC rd{}; rd.Format = gx_chain::TargetView(d.Format, x.srgb); rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        Need(rd.Format != DXGI_FORMAT_UNKNOWN && (d.BindFlags & D3D11_BIND_RENDER_TARGET) && d.SampleDesc.Count == 1, "theatre_target_format");
        Hr(device_->CreateRenderTargetView(t, &rd, &x.view), "theatre_target_view");
        if (targets_.size() >= 96) targets_.clear();
        targets_.push_back(x); return targets_.back();
    }
public:
    bool ready() const { return ps_ != nullptr; }
    bool mips() const { return mips_; }
    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }
    void close() {
        targets_.clear(); for (auto& i : image_) { i.view.Reset(); i.texture.Reset(); }
        width_ = height_ = 0; format_ = DXGI_FORMAT_UNKNOWN; mips_ = false;
        depth_.Reset(); raster_.Reset(); sampler_.Reset(); constants_.Reset(); ps_.Reset(); vs_.Reset(); state_.Reset(); device_.Reset();
    }
    void init(ID3D11Device* d) {
        if (ready()) return;
        device_ = d; ComPtr<ID3D11Device1> d1; Hr(d->QueryInterface(IID_PPV_ARGS(&d1)), "theatre_device1");
        const D3D_FEATURE_LEVEL level = d->GetFeatureLevel();
        Hr(d1->CreateDeviceContextState(0, &level, 1, D3D11_SDK_VERSION, __uuidof(ID3D11Device1), NULL, &state_), "theatre_state");
        Hr(d->CreateVertexShader(GxTheatreVs, sizeof(GxTheatreVs), NULL, &vs_), "theatre_vs");
        D3D11_BUFFER_DESC b{}; b.ByteWidth = sizeof(ScreenConstants); b.Usage = D3D11_USAGE_DEFAULT; b.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        Hr(d->CreateBuffer(&b, NULL, &constants_), "theatre_constants");
        D3D11_SAMPLER_DESC s{}; s.Filter = D3D11_FILTER_ANISOTROPIC; s.MaxAnisotropy = 8;
        s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; s.MaxLOD = D3D11_FLOAT32_MAX;
        Hr(d->CreateSamplerState(&s, &sampler_), "theatre_sampler");
        D3D11_RASTERIZER_DESC r{}; r.FillMode = D3D11_FILL_SOLID; r.CullMode = D3D11_CULL_NONE; r.DepthClipEnable = TRUE;
        Hr(d->CreateRasterizerState(&r, &raster_), "theatre_raster");
        D3D11_DEPTH_STENCIL_DESC z{}; z.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; Hr(d->CreateDepthStencilState(&z, &depth_), "theatre_depth");
        Hr(d->CreatePixelShader(GxTheatrePs, sizeof(GxTheatrePs), NULL, &ps_), "theatre_ps");
    }
    // Private per-eye images in the colour family of the game's eye image, stored as sRGB so that
    // filtering and the mip chain work on linear light. Recreated only when the eye size changes.
    void ensure(uint32_t w, uint32_t h, DXGI_FORMAT sourceFormat) {
        Need(ready() && w && h && w <= 8192 && h <= 8192, "theatre_extent");
        const gx_chain::Family family = gx_chain::FamilyOf(sourceFormat);
        Need(family == gx_chain::BGRA8 || family == gx_chain::RGBA8, "theatre_source_format");
        const DXGI_FORMAT format = family == gx_chain::BGRA8 ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        if (image_[0].texture && image_[1].texture && width_ == w && height_ == h && format_ == format) return;
        for (auto& i : image_) { i.view.Reset(); i.texture.Reset(); }
        targets_.clear(); width_ = height_ = 0;
        UINT support = 0;
        const bool mips = SUCCEEDED(device_->CheckFormatSupport(format, &support)) && (support & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN);
        D3D11_TEXTURE2D_DESC d{}; d.Width = w; d.Height = h; d.ArraySize = 1; d.SampleDesc.Count = 1; d.Format = format;
        d.MipLevels = mips ? 0u : 1u; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        d.MiscFlags = mips ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0u;
        for (auto& i : image_) {
            Hr(device_->CreateTexture2D(&d, NULL, &i.texture), "theatre_image");
            Hr(device_->CreateShaderResourceView(i.texture.Get(), NULL, &i.view), "theatre_image_view");
        }
        width_ = w; height_ = h; format_ = format; mips_ = mips;
    }
    ID3D11Texture2D* image(unsigned e) const { return e < 2 ? image_[e].texture.Get() : nullptr; }
    // Exact copy of a game eye image of the same size and colour family into the private image.
    void store(ID3D11DeviceContext* c, unsigned e, ID3D11Texture2D* source) {
        Need(e < 2 && image_[e].texture && source && source != image_[e].texture.Get(), "theatre_store");
        D3D11_TEXTURE2D_DESC s{}; source->GetDesc(&s);
        Need(s.Width == width_ && s.Height == height_ && s.SampleDesc.Count == 1 && s.ArraySize == 1 &&
             gx_chain::FamilyOf(s.Format) == gx_chain::FamilyOf(format_), "theatre_store_shape");
        c->CopySubresourceRegion(image_[e].texture.Get(), 0, 0, 0, 0, source, 0, NULL);
    }
    // Call after the private image of this eye holds the new frame.
    void filter(ID3D11DeviceContext* c, unsigned e) {
        Need(e < 2 && image_[e].view, "theatre_filter");
        if (mips_) c->GenerateMips(image_[e].view.Get());
    }
    // Black eye image with the screen of this eye drawn on it.
    void draw(ID3D11DeviceContext* c, ID3D11Texture2D* swapchainImage, unsigned e, const Corners& corners, const double* uv) {
        Need(ready() && c && swapchainImage && e < 2 && image_[e].view && uv && swapchainImage != image_[e].texture.Get(), "theatre_draw");
        Target& x = target(swapchainImage);
        D3D11_TEXTURE2D_DESC t{}; swapchainImage->GetDesc(&t);
        ScreenConstants k{};
        memcpy(k.corner, corners.clip, sizeof(k.corner));
        for (unsigned i = 0; i < 4; ++i) k.uv[i] = float(uv[i]);
        k.flags = x.srgb ? 0u : 1u;
        ComPtr<ID3D11DeviceContext1> c1; Hr(c->QueryInterface(IID_PPV_ARGS(&c1)), "theatre_context1");
        gx_chain::StateScope saved(c1.Get(), state_.Get()); Need(saved.valid(), "theatre_state_swap");
        c1->ClearState();
        const float black[4] = {0, 0, 0, 1};
        c1->ClearRenderTargetView(x.view.Get(), black);
        c1->UpdateSubresource(constants_.Get(), 0, NULL, &k, 0, 0);
        D3D11_VIEWPORT v{}; v.Width = float(t.Width); v.Height = float(t.Height); v.MaxDepth = 1;
        c1->RSSetViewports(1, &v); c1->RSSetState(raster_.Get());
        ID3D11RenderTargetView* rtv = x.view.Get(); c1->OMSetRenderTargets(1, &rtv, NULL); c1->OMSetDepthStencilState(depth_.Get(), 0);
        c1->IASetInputLayout(NULL); c1->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        c1->VSSetShader(vs_.Get(), NULL, 0); c1->PSSetShader(ps_.Get(), NULL, 0);
        ID3D11Buffer* cb = constants_.Get(); c1->VSSetConstantBuffers(0, 1, &cb); c1->PSSetConstantBuffers(0, 1, &cb);
        ID3D11SamplerState* sm = sampler_.Get(); c1->PSSetSamplers(0, 1, &sm);
        ID3D11ShaderResourceView* src = image_[e].view.Get(); c1->PSSetShaderResources(0, 1, &src);
        c1->Draw(4, 0);
        c1->ClearState();  // drop our references to the swapchain image from the owned state
    }
};
}
