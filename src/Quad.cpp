#include "Quad.h"
#include "Gfx.h"
#include "GfxState.h"
#include <SimpleMath.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;
using Microsoft::WRL::ComPtr;

namespace {
    constexpr const char* kTexturePath = ".\\Assets\\texture.png";
}

HRESULT Quad::Initialize()
{
    auto* device = Gfx::Dev();
    if (!device) return E_POINTER;

    // 頂点 / インデックス（VertexPositionTexture）
    VertexPositionTexture vertices[] = {
        { Vector3(-0.5f,  0.5f, 0.f), Vector2(0.f, 0.f) },
        { Vector3(0.5f,  0.5f, 0.f), Vector2(1.f, 0.f) },
        { Vector3(0.5f, -0.5f, 0.f), Vector2(1.f, 1.f) },
        { Vector3(-0.5f, -0.5f, 0.f), Vector2(0.f, 1.f) },
    };
    uint16_t indices[] = { 0,1,2, 0,2,3 };

    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.ByteWidth = sizeof(vertices);
    D3D11_SUBRESOURCE_DATA vbData{ vertices };
    HRESULT hr = device->CreateBuffer(&vbDesc, &vbData, m_vb.GetAddressOf());
    if (FAILED(hr)) return hr;

    D3D11_BUFFER_DESC ibDesc{};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.ByteWidth = sizeof(indices);
    D3D11_SUBRESOURCE_DATA ibData{ indices };
    hr = device->CreateBuffer(&ibDesc, &ibData, m_ib.GetAddressOf());
    if (FAILED(hr)) return hr;

    // BasicEffect
    m_effect = std::make_unique<BasicEffect>(device);
    m_effect->SetTextureEnabled(true);
    m_effect->SetLightingEnabled(false);

    const void* bc = nullptr; size_t bcLen = 0;
    m_effect->GetVertexShaderBytecode(&bc, &bcLen);
	// 入力レイアウト
    hr = device->CreateInputLayout(
        VertexPositionTexture::InputElements,
        VertexPositionTexture::InputElementCount,
        bc, bcLen, m_inputLayout.GetAddressOf());
    if (FAILED(hr)) return hr;

    // ★ CommonStates を Quad 内で生成
    m_states = std::make_unique<CommonStates>(device);

    // テクスチャ
    hr = m_texture.Load(kTexturePath);
    return hr;
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
    m_effect->SetWorld(XMMatrixIdentity());
    m_effect->SetView(XMMatrixIdentity());
    m_effect->SetProjection(wvp);
    m_effect->SetTexture(m_texture.GetSRV());
    m_effect->Apply(ctx);

    // ★ Quad 内の CommonStates を使用（半透明PNG想定）
    Gfx::SetAlphaNonPremul(ctx, m_states.get());
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
