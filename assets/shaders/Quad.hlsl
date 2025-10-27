cbuffer PerObject : register(b0)
{
    float4x4 gWVP; // ← 追加：ワールド×ビュー×プロジェクション
};

struct VSIn
{
    float3 pos : POSITION;
};
struct VSOut
{
    float4 svpos : SV_POSITION;
};

VSOut VS(VSIn v)
{
    VSOut o;
    // ここだけ変更：行列を掛ける
    o.svpos = mul(float4(v.pos, 1.0f), gWVP);
    return o;
}

float4 PS(VSOut i) : SV_Target
{
    // 色は今まで通り（必要ならお好きな色に）
    return float4(0.1, 0.8, 0.3, 1.0);
}
