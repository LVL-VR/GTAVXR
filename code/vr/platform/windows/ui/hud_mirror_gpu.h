#pragma once
// The mirror touches ONLY OM RTV/DSV bindings and OWN color/depth resources.
// Shader/CB/range/IA/viewport/scissor/stateblock/program/native viewport caches
// are neither set nor regenerated. Call immediately BEFORE a qualified native
// raw SF Draw, after its sole preparation, using its already-prepared bindings.
#include <d3d11.h>
#include "../../../core/ui/hud_mirror_policy.h"
namespace gx_hud_mirror_gpu
{
template <class T> inline void drop(T*& p)
{
    if (p)
        p->Release();
    p = 0;
}
inline bool colorFamily(DXGI_FORMAT resource, DXGI_FORMAT view)
{
    return ((resource == DXGI_FORMAT_R8G8B8A8_TYPELESS || resource == DXGI_FORMAT_R8G8B8A8_UNORM ||
             resource == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) &&
            (view == DXGI_FORMAT_R8G8B8A8_UNORM || view == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)) ||
           ((resource == DXGI_FORMAT_B8G8R8A8_TYPELESS || resource == DXGI_FORMAT_B8G8R8A8_UNORM ||
             resource == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) &&
            (view == DXGI_FORMAT_B8G8R8A8_UNORM || view == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB));
}
inline bool depthFamily(DXGI_FORMAT resource, DXGI_FORMAT view)
{
    return ((resource == DXGI_FORMAT_R24G8_TYPELESS || resource == DXGI_FORMAT_D24_UNORM_S8_UINT) &&
            view == DXGI_FORMAT_D24_UNORM_S8_UINT) ||
           ((resource == DXGI_FORMAT_R32G8X24_TYPELESS || resource == DXGI_FORMAT_D32_FLOAT_S8X24_UINT) &&
            view == DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
}
struct Source
{
    ID3D11Texture2D *color, *depth;
    ID3D11RenderTargetView* rtv;
    ID3D11DepthStencilView* dsv;
    ID3D11Device* device;
    D3D11_TEXTURE2D_DESC cd, dd;
    D3D11_RENDER_TARGET_VIEW_DESC rv;
    D3D11_DEPTH_STENCIL_VIEW_DESC dv;
    Source() : color(0), depth(0), rtv(0), dsv(0), device(0)
    {
        memset(&cd, 0, sizeof(cd));
        memset(&dd, 0, sizeof(dd));
        memset(&rv, 0, sizeof(rv));
        memset(&dv, 0, sizeof(dv));
    }
    ~Source()
    {
        close();
    }
    void close()
    {
        drop(rtv);
        drop(dsv);
        drop(color);
        drop(depth);
        drop(device);
    }
    template <class Context> bool read(Context* ctx)
    {
        close();
        if (!ctx || ctx->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)
            return false;
        ID3D11RenderTargetView* views[8] = {};
        ctx->OMGetRenderTargets(8, views, &dsv);
        rtv = views[0];
        bool only = rtv && dsv;
        for (unsigned i = 1; i < 8; ++i)
        {
            if (views[i])
                only = false;
            drop(views[i]);
        }
        if (!only)
        {
            close();
            return false;
        }
        ID3D11Resource *cr = 0, *dr = 0;
        rtv->GetResource(&cr);
        dsv->GetResource(&dr);
        HRESULT hc = cr ? cr->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&color) : E_FAIL;
        HRESULT hd = dr ? dr->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&depth) : E_FAIL;
        drop(cr);
        drop(dr);
        if (FAILED(hc) || FAILED(hd) || !color || !depth || color == depth)
        {
            close();
            return false;
        }
        color->GetDesc(&cd);
        depth->GetDesc(&dd);
        rtv->GetDesc(&rv);
        dsv->GetDesc(&dv);
        ctx->GetDevice(&device);
        ID3D11Device *cdev = 0, *ddev = 0;
        color->GetDevice(&cdev);
        depth->GetDevice(&ddev);
        const bool devices = device && device == cdev && device == ddev;
        drop(cdev);
        drop(ddev);
        // Deliberately finite first capability: 2D, single sample, typed DSV,
        // no readonly DSV flags. Multisample/arrays/mips are unsupported, not
        // guessed by a count-only/descriptor-family match.
        const bool finite =
            devices && cd.Width == dd.Width && cd.Height == dd.Height &&
            gx_hud_mirror::budget(cd.Width, cd.Height, 1) && cd.ArraySize == 1 && dd.ArraySize == 1 &&
            cd.MipLevels == 1 && dd.MipLevels == 1 && cd.SampleDesc.Count == 1 && dd.SampleDesc.Count == 1 &&
            cd.SampleDesc.Quality == 0 && dd.SampleDesc.Quality == 0 &&
            rv.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D && rv.Texture2D.MipSlice == 0 &&
            dv.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2D && dv.Texture2D.MipSlice == 0 && !dv.Flags &&
            colorFamily(cd.Format, rv.Format) && depthFamily(dd.Format, dv.Format);
        if (!finite)
        {
            close();
            return false;
        }
        return true;
    }

  private:
    Source(const Source&);
    Source& operator=(const Source&);
};
struct Surface
{
    ID3D11Texture2D *color, *depth;
    ID3D11RenderTargetView* rtv;
    ID3D11DepthStencilView* dsv;
    D3D11_TEXTURE2D_DESC cd, dd;
    D3D11_RENDER_TARGET_VIEW_DESC rv;
    D3D11_DEPTH_STENCIL_VIEW_DESC dv;
    Surface() : color(0), depth(0), rtv(0), dsv(0)
    {
        memset(&cd, 0, sizeof(cd));
        memset(&dd, 0, sizeof(dd));
        memset(&rv, 0, sizeof(rv));
        memset(&dv, 0, sizeof(dv));
    }
    ~Surface()
    {
        close();
    }
    void close()
    {
        drop(rtv);
        drop(dsv);
        drop(color);
        drop(depth);
    }
    bool matches(const Source& s) const
    {
        if (!color || !depth || !rtv || !dsv)
            return false;
        ID3D11Device *cdev = 0, *ddev = 0;
        color->GetDevice(&cdev);
        depth->GetDevice(&ddev);
        const bool sameDevice = cdev == s.device && ddev == s.device;
        drop(cdev);
        drop(ddev);
        return sameDevice && cd.Width == s.cd.Width && cd.Height == s.cd.Height && cd.Format == s.cd.Format &&
               dd.Format == s.dd.Format && !memcmp(&rv, &s.rv, sizeof(rv)) && !memcmp(&dv, &s.dv, sizeof(dv));
    }
    bool prepare(const Source& s)
    {
        if (matches(s))
            return color != s.color && depth != s.depth && color != s.depth && depth != s.color;
        // Caller has proved quiescent/unbound before recreating this OWN slot.
        close();
        cd = s.cd;
        dd = s.dd;
        rv = s.rv;
        dv = s.dv;
        cd.Usage = dd.Usage = D3D11_USAGE_DEFAULT;
        cd.CPUAccessFlags = dd.CPUAccessFlags = 0;
        cd.MiscFlags = dd.MiscFlags = 0;
        cd.BindFlags = D3D11_BIND_RENDER_TARGET;
        dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        if (!s.device || FAILED(s.device->CreateTexture2D(&cd, 0, &color)) || !color ||
            FAILED(s.device->CreateTexture2D(&dd, 0, &depth)) || !depth ||
            FAILED(s.device->CreateRenderTargetView(color, &rv, &rtv)) || !rtv ||
            FAILED(s.device->CreateDepthStencilView(depth, &dv, &dsv)) || !dsv)
        {
            close();
            return false;
        }
        if (color == s.color || depth == s.depth || color == s.depth || depth == s.color)
        {
            close();
            return false;
        }
        return true;
    }

  private:
    Surface(const Surface&);
    Surface& operator=(const Surface&);
};
template <class Context> struct OmScope
{
    Context* ctx;
    ID3D11RenderTargetView* views[8];
    ID3D11DepthStencilView* depth;
    bool changed;
    explicit OmScope(Context* c) : ctx(c), depth(0), changed(false)
    {
        memset(views, 0, sizeof(views));
        if (ctx)
            ctx->OMGetRenderTargets(8, views, &depth);
    }
    bool is(const Source& s) const
    {
        if (views[0] != s.rtv || depth != s.dsv)
            return false;
        for (unsigned i = 1; i < 8; ++i)
            if (views[i])
                return false;
        return true;
    }
    bool matches() const
    {
        ID3D11RenderTargetView* v[8] = {};
        ID3D11DepthStencilView* d = 0;
        ctx->OMGetRenderTargets(8, v, &d);
        bool same = d == depth;
        for (unsigned i = 0; i < 8; ++i)
        {
            same = same && v[i] == views[i];
            drop(v[i]);
        }
        drop(d);
        return same;
    }
    bool restore()
    {
        if (changed)
        {
            ctx->OMSetRenderTargets(8, views, depth);
            changed = false;
        }
        return ctx && matches();
    }
    ~OmScope()
    {
        if (changed)
            ctx->OMSetRenderTargets(8, views, depth);
        for (unsigned i = 0; i < 8; ++i)
            drop(views[i]);
        drop(depth);
    }
};
template <class Context> inline bool outputsUnbound(Context* ctx, const Surface& s)
{
    if (!ctx)
        return false;
    ID3D11RenderTargetView* v[8] = {};
    ID3D11DepthStencilView* d = 0;
    ctx->OMGetRenderTargets(8, v, &d);
    // Alternate views of the same OWN resource also keep it bound. Pointer
    // comparison of the one cached RTV/DSV alone is not a lifetime proof.
    bool ok = true;
    ID3D11Resource* r = 0;
    if (d)
    {
        d->GetResource(&r);
        ok = r != (ID3D11Resource*)s.color && r != (ID3D11Resource*)s.depth;
        drop(r);
    }
    for (unsigned i = 0; i < 8; ++i)
    {
        if (v[i])
        {
            v[i]->GetResource(&r);
            ok = ok && r != (ID3D11Resource*)s.color && r != (ID3D11Resource*)s.depth;
            drop(r);
        }
        drop(v[i]);
    }
    drop(d);
    return ok;
}
template <class Context> inline bool noWritableSideOutputs(Context* ctx)
{
    // A duplicate Draw must not increment SO buffers or unordered counters.
    // These are rejected; there is no attempt to guess/restore hidden counters.
    ID3D11Buffer* so[4] = {};
    ctx->SOGetTargets(4, so);
    bool safe = true;
    for (unsigned i = 0; i < 4; ++i)
    {
        safe = safe && !so[i];
        drop(so[i]);
    }
    ID3D11UnorderedAccessView *om[8] = {}, *cs[8] = {};
    ctx->OMGetRenderTargetsAndUnorderedAccessViews(0, 0, 0, 0, 8, om);
    ctx->CSGetUnorderedAccessViews(0, 8, cs);
    for (unsigned i = 0; i < 8; ++i)
    {
        safe = safe && !om[i] && !cs[i];
        drop(om[i]);
        drop(cs[i]);
    }
    ID3D11GeometryShader* gs = 0;
    ID3D11HullShader* hs = 0;
    ID3D11DomainShader* ds = 0;
    ID3D11ComputeShader* cshader = 0;
    ctx->GSGetShader(&gs, 0, 0);
    ctx->HSGetShader(&hs, 0, 0);
    ctx->DSGetShader(&ds, 0, 0);
    ctx->CSGetShader(&cshader, 0, 0);
    safe = safe && !gs && !hs && !ds && !cshader;
    drop(gs);
    drop(hs);
    drop(ds);
    drop(cshader);
    ID3D11Predicate* predicate = 0;
    BOOL expected = FALSE;
    ctx->GetPredication(&predicate, &expected);
    safe = safe && !predicate;
    drop(predicate);
    return safe;
}
template <class Context> inline bool noInputAlias(Context* ctx, const Surface& s)
{
    ID3D11ShaderResourceView* views[128] = {};
    bool safe = true;
    for (unsigned stage = 0; stage < 6; ++stage)
    {
        switch (stage)
        {
        case 0:
            ctx->VSGetShaderResources(0, 128, views);
            break;
        case 1:
            ctx->PSGetShaderResources(0, 128, views);
            break;
        case 2:
            ctx->GSGetShaderResources(0, 128, views);
            break;
        case 3:
            ctx->HSGetShaderResources(0, 128, views);
            break;
        case 4:
            ctx->DSGetShaderResources(0, 128, views);
            break;
        default:
            ctx->CSGetShaderResources(0, 128, views);
            break;
        }
        for (unsigned i = 0; i < 128; ++i)
        {
            if (views[i])
            {
                ID3D11Resource* r = 0;
                views[i]->GetResource(&r);
                if (r == (ID3D11Resource*)s.color || r == (ID3D11Resource*)s.depth)
                    safe = false;
                drop(r);
            }
            drop(views[i]);
        }
    }
    return safe;
}
template <class Context> inline bool begin(Context* ctx, const Source& src, Surface& dst)
{
    OmScope<Context> om(ctx);
    if (!om.is(src) || !outputsUnbound(ctx, dst) || dst.color == src.color || dst.depth == src.depth ||
        !dst.matches(src) || !noInputAlias(ctx, dst))
        return false;
    const float empty[4] = {0, 0, 0, 0};
    ctx->ClearRenderTargetView(dst.rtv, empty);
    // Actual ancestor stencil/depth at THIS original subtree entry; read-only
    // original source -> OWN destination. Internal masks/clears mirror later.
    ctx->CopyResource(dst.depth, src.depth);
    return om.matches();
}
template <class Context>
inline bool draw(Context* ctx, const Source& src, Surface& dst, unsigned kind, unsigned count, unsigned start,
                 int base, bool& restored)
{
    restored = true;
    OmScope<Context> om(ctx);
    if (!om.is(src) || !count || count > 0x1000000u || (kind != 1 && kind != 2) || !dst.matches(src) ||
        dst.color == src.color || dst.depth == src.depth || !noWritableSideOutputs(ctx) ||
        !noInputAlias(ctx, dst))
        return false;
    ID3D11RenderTargetView* target = dst.rtv;
    ctx->OMSetRenderTargets(1, &target, dst.dsv);
    om.changed = true;
    // Do NOT call grcDevice::Draw/SetUpPriorToDraw/BeginVertices here. Native
    // effect/program caches, CB backing/dirty/ranges and rings remain untouched.
    if (kind == 1)
        ctx->Draw(count, start);
    else
        ctx->DrawIndexed(count, start, base);
    restored = om.restore();
    return restored;
}
template <class Context>
inline bool clear(Context* ctx, const Source& src, Surface& dst, unsigned flags, const float* color,
                  float depth, unsigned stencil)
{
    OmScope<Context> om(ctx);
    if (!om.is(src) || !dst.matches(src) || dst.color == src.color || dst.depth == src.depth)
        return false;
    if (flags & 1)
    {
        if (!color)
            return false;
        ctx->ClearRenderTargetView(dst.rtv, color);
    }
    unsigned dflags = 0;
    if (flags & 2)
        dflags |= D3D11_CLEAR_DEPTH;
    if (flags & 4)
        dflags |= D3D11_CLEAR_STENCIL;
    if (dflags)
        ctx->ClearDepthStencilView(dst.dsv, dflags, depth, (UINT8)stencil);
    return om.matches();
}
}
