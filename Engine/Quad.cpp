#include "Quad.h"
#include "Gfx.h"
#include "GfxState.h"
#include <SimpleMath.h>
#include <Effects.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;
using Microsoft::WRL::ComPtr;

namespace {
    constexpr const char* kTexturePath = ".\\Assets\\texture.png";
}

HRESULT Quad::Initialize()
{
  return Initialize(0.f, 0.f, 1.f, 1.f); 
}

HRESULT Quad::Initialize(float u0, float v0, float u1, float v1)
{
    auto* device = Gfx::Dev();
    if (!device) return E_POINTER;

    // IB（固定）
    const uint16_t idx[6] = { 0,1,2, 0,2,3 };
    D3D11_BUFFER_DESC ibd{};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.ByteWidth = sizeof(idx);
    D3D11_SUBRESOURCE_DATA idata{ idx };
    HRESULT hr = device->CreateBuffer(&ibd, &idata, m_ib.GetAddressOf());
    if (FAILED(hr)) return hr;

    // VB（UVは引数で決定）
    hr = BuildVB(u0, v0, u1, v1);
    if (FAILED(hr)) return hr;

    // BasicEffect
    auto ef = std::make_shared<BasicEffect>(device);
    ef->SetTextureEnabled(true);
    ef->SetLightingEnabled(false);
	m_effect = ef;

    //std::shared_ptr<AlphaTestEffect> effect;
    auto alpha = std::make_shared<DirectX::AlphaTestEffect>(Gfx::Dev());
    alpha->SetReferenceAlpha(128);
    alpha->SetAlphaFunction(D3D11_COMPARISON_GREATER);
    



    // InputLayout
    const void* bc = nullptr; size_t len = 0;
    m_effect->GetVertexShaderBytecode(&bc, &len);
    hr = device->CreateInputLayout(
        VertexPositionTexture::InputElements,
        VertexPositionTexture::InputElementCount,
        bc, len, m_inputLayout.GetAddressOf());
    if (FAILED(hr)) return hr;

    // States
    m_states = std::make_unique<CommonStates>(device);
    return S_OK;
}

void Quad::Draw(const XMMATRIX& wvp)
{
    auto* ctx = Gfx::Ctx();
    if (!ctx) return;

    // IA
    UINT stride = sizeof(VertexPositionTexture), offset = 0;
    ID3D11Buffer* vb = m_vb.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(m_ib.Get(), DXGI_FORMAT_R16_UINT, 0);
    ctx->IASetInputLayout(m_inputLayout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Effect 設定（W=I, V=I, P=WVP）
    // dynamic_pointer_cast を使う（安全・推奨）
    std::shared_ptr<DirectX::BasicEffect> be = std::dynamic_pointer_cast<DirectX::BasicEffect>(m_effect);

    be->SetWorld(XMMatrixIdentity());
    be->SetView(XMMatrixIdentity());
    be->SetProjection(wvp);
    be->SetTexture(m_texture.GetSRV());
    be->Apply(ctx);



    // ★ Quad 内の CommonStates を使用（半透明PNG想定）
    //Gfx::SetAlphaNonPremul(ctx, m_states.get());
    Gfx::SetAlphaNonPremulWriteZ(ctx, m_states.get());
    ID3D11SamplerState* samp = m_states->LinearWrap();
    ctx->PSSetSamplers(0, 1, &samp);

    // SRV
    if (auto* srv = m_texture.GetSRV())
        ctx->PSSetShaderResources(0, 1, &srv);

    // Draw
    ctx->DrawIndexed(6, 0, 0);

    //// 後片付け（任意）
    //ID3D11ShaderResourceView* nullSRV = nullptr;
    //ctx->PSSetShaderResources(0, 1, &nullSRV);
    Gfx::SetOpaque(ctx, m_states.get());
}

// UV指定版 VB 作成
HRESULT Quad::BuildVB(float u0, float v0, float u1, float v1)
{
    auto* device = Gfx::Dev();
    if (!device) return E_POINTER;

    VertexPositionTexture v[4] = {
        { Vector3(-0.5f,  0.5f, 0.f), Vector2(u0, v0) }, // 左上
        { Vector3(0.5f,  0.5f, 0.f), Vector2(u1, v0) }, // 右上
        { Vector3(0.5f, -0.5f, 0.f), Vector2(u1, v1) }, // 右下
        { Vector3(-0.5f, -0.5f, 0.f), Vector2(u0, v1) }, // 左下
    };

    m_vb.Reset();
    D3D11_BUFFER_DESC vbd{};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.ByteWidth = sizeof(v);
    D3D11_SUBRESOURCE_DATA vdata{ v };
    return device->CreateBuffer(&vbd, &vdata, m_vb.GetAddressOf());
}

HRESULT Quad::LoadTexture(const std::string& path)
{
    return m_texture.Load(path); // DXTK WIC ローダ（Texture.cppで実装済み）
}
