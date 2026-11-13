// Same angular scene crop, with separately captured native reticle pixels.
Texture2DArray<float4> ScopeSource : register(t0);
Texture2D<float4> ScopeReticle : register(t1);
SamplerState ScopeSampler : register(s0);
// Unused .w lanes preserve the 48-byte mapping ABI:
// RowU.w/RowV.w = aspect-preserving reticle UV scale; RowQ.w = alpha convention.
cbuffer ScopeMapping : register(b0) {float4 RowU;float4 RowV;float4 RowQ;};
float4 main(float4 position : SV_POSITION,float2 lensUV : TEXCOORD0) : SV_TARGET {
 float2 lens=float2(lensUV.x*2-1,1-lensUV.y*2);float3 p=float3(lens,1);float q=dot(RowQ.xyz,p);
 if(dot(lens,lens)>1||q<=1e-8)return float4(0,0,0,0);
 float2 uv=float2(dot(RowU.xyz,p),dot(RowV.xyz,p))/q;
 if(any(uv<0)||any(uv>1))return float4(0,0,0,0);
 float3 color=ScopeSource.Sample(ScopeSampler,float3(uv,0)).rgb;
 if(RowQ.w>.5){
  float2 ruv=(lensUV-.5)*float2(RowU.w,RowV.w)+.5;
  if(all(ruv>=0)&&all(ruv<=1)){
   float4 mark=ScopeReticle.Sample(ScopeSampler,ruv);
   color=color*(1-mark.a)+(RowQ.w>1.5?mark.rgb:mark.rgb*mark.a);
  }
 }
 return float4(color,1);
}
