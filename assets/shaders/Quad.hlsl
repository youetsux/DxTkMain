cbuffer CBColor : register(b0) { float4 gColor; }
struct VSIn  { float3 pos : POSITION; };
struct VSOut { float4 svpos : SV_POSITION; };
VSOut VS(VSIn v){ VSOut o; o.svpos=float4(v.pos,1.0); return o; }
float4 PS(VSOut i) : SV_Target { return gColor; }