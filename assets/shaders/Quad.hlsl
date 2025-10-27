// Quad.hlsl  --- テクスチャ貼り付け用最小シェーダ

cbuffer PerObject : register(b0)
{
    float4x4 gWVP; // World * View * Projection
};

Texture2D gTex : register(t0);
SamplerState gSamp : register(s0);

struct VSIn
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD0;
};

struct VSOut
{
    float4 svpos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOut VS(VSIn vin)
{
    VSOut vout;
    vout.svpos = mul(float4(vin.pos, 1.0f), gWVP);
    vout.uv = vin.uv; // 必要なら vout.uv.y = 1.0 - vin.uv.y;
    return vout;
}

float4 PS(VSOut pin) : SV_Target
{
    // 画像そのまま表示（必要になったら色乗算やアルファ処理を追加）
    //return float4(pin.uv.x, pin.uv.y, 0, 1);
    return gTex.Sample(gSamp, pin.uv);
}
