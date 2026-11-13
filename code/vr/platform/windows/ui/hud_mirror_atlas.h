#pragma once
// An OWN copy destination only (BindFlags=0): never bound, drawn or advanced.
// Mirror GPU3 remains unchanged. Call only within its same busy render scope.
#include "hud_mirror_gpu.h"
#include "../../../core/ui/hud_mirror_after_policy.h"
namespace gx_hud_after
{
struct Atlas
{
    ID3D11Texture2D* color;
    ID3D11Device* device;
    DXGI_FORMAT format;
    Atlas() : color(0), device(0), format(DXGI_FORMAT_UNKNOWN) {}
    ~Atlas()
    {
        close();
    }
    void close()
    {
        gx_hud_mirror_gpu::drop(color);
        gx_hud_mirror_gpu::drop(device);
        format = DXGI_FORMAT_UNKNOWN;
    }
    template <class Context> bool unbound(Context* ctx) const
    {
        if (!color)
            return true;
        if (!ctx)
            return false;
        ID3D11RenderTargetView* v[8] = {};
        ID3D11DepthStencilView* d = 0;
        ctx->OMGetRenderTargets(8, v, &d);
        bool okay = true;
        ID3D11Resource* r = 0;
        if (d)
        {
            d->GetResource(&r);
            okay = r != (ID3D11Resource*)color;
            gx_hud_mirror_gpu::drop(r);
        }
        for (unsigned i = 0; i < 8; ++i)
        {
            if (v[i])
            {
                v[i]->GetResource(&r);
                okay = okay && r != (ID3D11Resource*)color;
                gx_hud_mirror_gpu::drop(r);
            }
            gx_hud_mirror_gpu::drop(v[i]);
        }
        gx_hud_mirror_gpu::drop(d);
        return okay;
    }
    template <class Context> bool prepare(Context* ctx, const gx_hud_mirror_gpu::Source& s)
    {
        if (!ctx || !s.device || !s.color || !s.depth || !unbound(ctx))
            return false;
        if (color && device == s.device && format == s.cd.Format)
            return color != s.color && color != s.depth;
        close();
        D3D11_TEXTURE2D_DESC desc = s.cd;
        desc.Width = AtlasWidth;
        desc.Height = AtlasHeight;
        desc.MipLevels = desc.ArraySize = 1;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = desc.CPUAccessFlags = desc.MiscFlags = 0;
        if (FAILED(s.device->CreateTexture2D(&desc, 0, &color)) || !color || color == s.color ||
            color == s.depth)
        {
            close();
            return false;
        }
        device = s.device;
        device->AddRef();
        format = desc.Format;
        return true;
    }
    template <class Context>
    bool copy(Context* ctx, const gx_hud_mirror_gpu::Source& s, const gx_hud_mirror_gpu::Surface* surfaces,
              const Pack& p, bool& fatal)
    {
        fatal = false;
        if (!ctx || ctx->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE || !p.ready() || !surfaces || !color ||
            device != s.device || format != s.cd.Format || !gx_hud_mirror_gpu::noWritableSideOutputs(ctx))
            return false;
        if (color == s.color || color == s.depth || !unbound(ctx) || FAILED(device->GetDeviceRemovedReason()))
        {
            fatal = true;
            return false;
        }
        ID3D11Device* current = 0;
        ctx->GetDevice(&current);
        const bool same = current == device;
        gx_hud_mirror_gpu::drop(current);
        if (!same)
            return false;
        // Validate EVERY source/box before the first OWN write. Undefined atlas
        // padding is never published; every item rectangle is overwritten fully.
        for (unsigned i = 0; i < p.count; ++i)
        {
            if (p.slot[i] >= gx_hud_mirror::MaxTargets)
                return false;
            const gx_hud_mirror_gpu::Surface& src = surfaces[p.slot[i]];
            if (!src.matches(s))
                return false;
            if (src.color == color || src.color == s.color || src.color == s.depth || src.depth == s.color ||
                src.depth == s.depth || !gx_hud_mirror_gpu::outputsUnbound(ctx, src))
            {
                fatal = true;
                return false;
            }
            if (!gx_native_ui::bounded(p.source[i], s.cd.Width, s.cd.Height) ||
                !gx_native_ui::bounded(p.items[i], AtlasWidth, AtlasHeight) ||
                p.items[i].width != p.source[i].width || p.items[i].height != p.source[i].height ||
                p.items[i].id != p.source[i].id || p.items[i].priority != p.source[i].priority ||
                p.items[i].route != p.source[i].route ||
                !(gx_native_ui::gameplayCrop(p.source[i], s.cd.Width, s.cd.Height) ||
                  gx_native_ui::wantedCrop(p.source[i], s.cd.Width, s.cd.Height)))
                return false;
        }
        for (unsigned i = 0; i < p.count; ++i)
        {
            const GxUiItem& crop = p.source[i];
            D3D11_BOX box = {crop.x, crop.y, 0, crop.x + crop.width, crop.y + crop.height, 1};
            ctx->CopySubresourceRegion(color, 0, p.items[i].x, p.items[i].y, 0, surfaces[p.slot[i]].color, 0,
                                       &box);
        }
        if (FAILED(device->GetDeviceRemovedReason()) || !unbound(ctx))
        {
            fatal = true;
            return false;
        }
        return true;
    }

  private:
    Atlas(const Atlas&);
    Atlas& operator=(const Atlas&);
};
}
