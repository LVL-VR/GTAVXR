#pragma once
// Minimal derivative of the qualified HUD mirror: SAME prepared native shader,
// vertices, CB ranges, viewport and scissor. ONLY own OM outputs and alpha blend
// change temporarily. Native flat draw has already happened exactly once.
#include "../ui/hud_mirror_gpu.h"
namespace gx_scope_reticle_mirror
{
using gx_hud_mirror_gpu::Source;
using gx_hud_mirror_gpu::Surface;
class Blend
{
    ID3D11Device* device_;
    ID3D11BlendState* state_;
    D3D11_BLEND_DESC input_;
    Blend(const Blend&);
    Blend& operator=(const Blend&);

  public:
    Blend() : device_(NULL), state_(NULL)
    {
        memset(&input_, 0, sizeof(input_));
    }
    ~Blend()
    {
        close();
    }
    void close()
    {
        gx_hud_mirror_gpu::drop(state_);
        gx_hud_mirror_gpu::drop(device_);
        memset(&input_, 0, sizeof(input_));
    }
    ID3D11BlendState* prepare(ID3D11Device* device, ID3D11BlendState* original)
    {
        if (!device || !original)
            return NULL;
        D3D11_BLEND_DESC desc = {};
        original->GetDesc(&desc);
        const D3D11_RENDER_TARGET_BLEND_DESC& rt = desc.RenderTarget[0];
        if (desc.AlphaToCoverageEnable || desc.IndependentBlendEnable || !rt.BlendEnable ||
            rt.SrcBlend != D3D11_BLEND_SRC_ALPHA || rt.DestBlend != D3D11_BLEND_INV_SRC_ALPHA ||
            rt.BlendOp != D3D11_BLEND_OP_ADD || rt.SrcBlendAlpha != D3D11_BLEND_SRC_ALPHA ||
            rt.DestBlendAlpha != D3D11_BLEND_INV_SRC_ALPHA || rt.BlendOpAlpha != D3D11_BLEND_OP_ADD ||
            rt.RenderTargetWriteMask != D3D11_COLOR_WRITE_ENABLE_ALL)
            return NULL;
        if (device_ == device && state_ && !memcmp(&input_, &desc, sizeof(desc)))
            return state_;
        close();
        D3D11_BLEND_DESC owned = desc;
        owned.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        if (FAILED(device->CreateBlendState(&owned, &state_)))
            return NULL;
        device_ = device;
        device_->AddRef();
        input_ = desc;
        return state_;
    }
};
template <class Context>
bool draw(Context* ctx, const Source& src, Surface& dst, Blend& blend, unsigned kind, unsigned count,
          unsigned start, int base, bool queriesSafe, bool& restored)
{
    restored = true;
    gx_hud_mirror_gpu::OmScope<Context> om(ctx);
    if (!queriesSafe || !ctx || !om.is(src) || !count || count > 0x1000000u || (kind != 1 && kind != 2) ||
        !dst.matches(src) || dst.color == src.color || dst.depth == src.depth ||
        !gx_hud_mirror_gpu::noWritableSideOutputs(ctx) || !gx_hud_mirror_gpu::noInputAlias(ctx, dst))
        return false;
    ID3D11BlendState* original = NULL;
    float factors[4] = {};
    UINT mask = 0;
    ctx->OMGetBlendState(&original, factors, &mask);
    ID3D11BlendState* corrected = blend.prepare(src.device, original);
    if (!corrected)
    {
        gx_hud_mirror_gpu::drop(original);
        return false;
    }
    ID3D11RenderTargetView* target = dst.rtv;
    ctx->OMSetRenderTargets(1, &target, dst.dsv);
    om.changed = true;
    ctx->OMSetBlendState(corrected, factors, mask);
    if (kind == 1)
        ctx->Draw(count, start);
    else
        ctx->DrawIndexed(count, start, base);
    ctx->OMSetBlendState(original, factors, mask);
    restored = om.restore();
    ID3D11BlendState* actual = NULL;
    float afterFactors[4] = {};
    UINT afterMask = 0;
    ctx->OMGetBlendState(&actual, afterFactors, &afterMask);
    restored = restored && actual == original && afterMask == mask &&
               !memcmp(factors, afterFactors, sizeof(factors));
    gx_hud_mirror_gpu::drop(actual);
    gx_hud_mirror_gpu::drop(original);
    return restored;
}
}
