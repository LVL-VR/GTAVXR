 
// Compiled offline with the Windows SDK fxc (tools/buildneural/build_chain_shaders.ps1); the bridge embeds only the bytes.
// Matrices use the default column_major packing, which reads the bridge's column-major float[16] (v' = M * v) unchanged.

cbuffer Capture : register(b0)
{
    float4x4 RelFromClip;      // camera-relative guides of this eye (eye_chain.h GxEyeGuides), without jitter
    float4x4 PrevClipFromRel;  // the previous pair of the same eye at the same origin
    float2 Size;               // render pixels
    float2 Jitter;             // measured image shift of this pair's jitter, render pixels, +x right, +y down
    uint Reset;                // 1 = no history: zero motion
    uint3 CapturePad;
};
Texture2D<float4> EyeColor : register(t0);
Texture2D<float> EyeDepth : register(t1);
RWTexture2D<float4> ColorOut : register(u0);
RWTexture2D<float> DepthOut : register(u1);
RWTexture2D<float2> MotionOut : register(u2);

// Colour BGRA8 -> RGBA16F (display-encoded, alpha 1), reverse-Z depth -> R32F, camera motion RG16F in render pixels:
 
[numthreads(8, 8, 1)] void CsCapture(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= (uint)Size.x || id.y >= (uint)Size.y)
        return;
    ColorOut[id.xy] = float4(EyeColor[id.xy].rgb, 1);
    const float d = EyeDepth[id.xy];
    DepthOut[id.xy] = d;
    float2 motion = 0;
    if (!Reset)
    {
        const float2 u = float2(id.xy) + 0.5 - Jitter;
        const float2 ndc = float2(u.x / Size.x * 2 - 1, 1 - u.y / Size.y * 2);
        const float4 rel = mul(RelFromClip, float4(ndc, d, 1));   // homogeneous: the far plane / sky stays a direction
        const float4 prev = mul(PrevClipFromRel, rel);
        if (abs(prev.w) > 1e-20)
        {
            const float2 p = prev.xy / prev.w;
            motion = float2((p.x + 1) * 0.5 * Size.x, (1 - p.y) * 0.5 * Size.y) - u;
        }
    }
    MotionOut[id.xy] = motion;
}

cbuffer Blit : register(b0)
{
    float4 SrcRect;    // x, y, width, height in source pixels
    float2 SrcSize;    // source texture size
    float2 DstOrigin;  // viewport top-left in the target (SV_Position includes it)
    float2 DstSize;    // viewport size
    uint Flags;        // bit0 bilinear (else exact Load), bit1 linearize (sRGB target view of display-encoded data), bit2 opaque
    uint BlitPad;
};
Texture2D<float4> Source : register(t0);
SamplerState Linear : register(s0);

float4 VsFull(uint id : SV_VertexID) : SV_Position
{
    const float2 uv = float2((id << 1) & 2, id & 2);
    return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}
float3 ToLinear(float3 c)
{
    return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}
float4 PsBlit(float4 p : SV_Position) : SV_Target
{
    const float2 local = p.xy - DstOrigin;
    float4 c;
    if (Flags & 1)
        c = Source.SampleLevel(Linear, (SrcRect.xy + local / DstSize * SrcRect.zw) / SrcSize, 0);
    else
        c = Source.Load(int3(int2(local) + int2(SrcRect.xy), 0));
    if (Flags & 2)
        c.rgb = ToLinear(saturate(c.rgb));
    if (Flags & 4)
        c.a = 1;
    return c;
}
