// Owned per-eye source array view (ONE slice per SRV). Independent crop
// prototype only: no new detail or lens-origin translational parallax.
Texture2DArray<float4> ScopeSource : register(t0);
SamplerState ScopeSampler : register(s0);
cbuffer ScopeMapping : register(b0) {float4 RowU;float4 RowV;float4 RowQ;};
float4 main(float4 position : SV_POSITION,float2 lensUV : TEXCOORD0) : SV_TARGET {
 float2 lens=float2(lensUV.x*2-1,1-lensUV.y*2);float3 p=float3(lens,1);float q=dot(RowQ.xyz,p);
 if(dot(lens,lens)>1||q<=1e-8)return float4(0,0,0,0);
 float2 uv=float2(dot(RowU.xyz,p),dot(RowV.xyz,p))/q;
 if(any(uv<0)||any(uv>1))return float4(0,0,0,0);
 float4 color=ScopeSource.Sample(ScopeSampler,float3(uv,0));return float4(color.rgb,1);
}
