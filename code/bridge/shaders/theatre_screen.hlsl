// Stereo theatre screen. GTAVXR-owned. Compiled offline with the Windows SDK fxc
// (tools/build-theatre-shaders.cmd); the bridge embeds only the bytes.
// One quad per eye: clip-space corners computed by the bridge, the eye's own cutscene image on it.

cbuffer Screen : register(b0)
{
    float4 Corner[4];  // clip space: top-left, top-right, bottom-left, bottom-right
    float4 UvRect;     // u0, v0, u1, v1 of the authored frame inside the eye image
    uint Flags;        // bit0: the target view stores display values, so encode before writing
    uint3 ScreenPad;
};
Texture2D<float4> Image : register(t0);
SamplerState Smooth : register(s0);

struct ScreenVertex
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float2 edge : TEXCOORD1;  // 0..1 across the screen, for a one-pixel soft border
};

ScreenVertex VsScreen(uint id : SV_VertexID)
{
    ScreenVertex o;
    const float2 corner = float2(id & 1, (id >> 1) & 1);
    o.position = Corner[id & 3];
    o.uv = lerp(UvRect.xy, UvRect.zw, corner);
    o.edge = corner;
    return o;
}

float3 ToDisplay(float3 c)
{
    return c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}

float4 PsScreen(ScreenVertex v) : SV_Target
{
    float3 c = Image.Sample(Smooth, v.uv).rgb;
    const float2 inside = min(v.edge, 1 - v.edge) / max(fwidth(v.edge), 1e-6);
    c *= saturate(min(inside.x, inside.y));
    if (Flags & 1)
        c = ToDisplay(saturate(c));
    return float4(c, 1);
}
