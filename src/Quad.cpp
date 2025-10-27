#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdexcept>
#include <wrl/client.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

#include "Quad.h"
#include "Gfx.h" // ← Dev/Ctx/… を使う
using namespace DirectX;
using Microsoft::WRL::ComPtr;

static inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr)) {
        OutputDebugStringA("HRESULT failed in Quad\n");
        throw std::runtime_error("HRESULT failed in Quad");
    }
}

void Quad::Initialize()
{
    // Gfx::Init 済み前提
    auto dev = Gfx::Dev();

    // === VB/IB ===
    struct V { float x, y, z; };
    V vertices[4] = {
        { -0.5f,  0.5f, 0.0f },
        {  0.5f,  0.5f, 0.0f },
        {  0.5f, -0.5f, 0.0f },
        { -0.5f, -0.5f, 0.0f },
    };
    uint16_t indices[6] = { 0,1,2, 0,2,3 };

    D3D11_BUFFER_DESC vbd{};
    vbd.ByteWidth = sizeof(vertices);
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vinit{ vertices, 0, 0 };
    ThrowIfFailed(dev->CreateBuffer(&vbd, &vinit, m_vb.GetAddressOf()));

    D3D11_BUFFER_DESC ibd{};
    ibd.ByteWidth = sizeof(indices);
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA iinit{ indices, 0, 0 };
    ThrowIfFailed(dev->CreateBuffer(&ibd, &iinit, m_ib.GetAddressOf()));

    // === シェーダ（既存 Quad.hlsl を使う：VS/PS） ===

    ComPtr<ID3DBlob> vsb, psb, err;
    ThrowIfFailed(D3DCompileFromFile(
        L"assets/shaders/Quad.hlsl", nullptr, nullptr,
        "VS", "vs_5_0", 0, 0, vsb.GetAddressOf(), err.GetAddressOf()));
    err.Reset();
    ThrowIfFailed(D3DCompileFromFile(
        L"assets/shaders/Quad.hlsl", nullptr, nullptr,
        "PS", "ps_5_0", 0, 0, psb.GetAddressOf(), err.GetAddressOf()));

    ThrowIfFailed(dev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(),
        nullptr, m_vs.GetAddressOf()));
    ThrowIfFailed(dev->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(),
        nullptr, m_ps.GetAddressOf()));

    // 入力レイアウト（POSITIONのみ）
    D3D11_INPUT_ELEMENT_DESC il[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    ThrowIfFailed(dev->CreateInputLayout(il, _countof(il),
        vsb->GetBufferPointer(), vsb->GetBufferSize(), m_layout.GetAddressOf()));

    // === 定数バッファ（b0: 4x4行列） ===
    D3D11_BUFFER_DESC cbd{};
    cbd.ByteWidth = sizeof(XMFLOAT4X4);
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ThrowIfFailed(dev->CreateBuffer(&cbd, nullptr, m_cbPerObject.GetAddressOf()));
}

void Quad::Draw(const XMMATRIX& wvp)
{
    auto ctx = Gfx::Ctx();

    // WVP を転置して b0 に送る（HLSLが列優先）
    XMFLOAT4X4 m;
    XMStoreFloat4x4(&m, XMMatrixTranspose(wvp));
    ctx->UpdateSubresource(m_cbPerObject.Get(), 0, nullptr, &m, 0, 0);
    ID3D11Buffer* cb = m_cbPerObject.Get();
    ctx->VSSetConstantBuffers(0, 1, &cb);

    // パイプライン（Quad の責務）
    ctx->IASetInputLayout(m_layout.Get());
    ctx->VSSetShader(m_vs.Get(), nullptr, 0);
    ctx->PSSetShader(m_ps.Get(), nullptr, 0);

    // ジオメトリ
    UINT stride = sizeof(float) * 3, offset = 0;
    ID3D11Buffer* vb = m_vb.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(m_ib.Get(), DXGI_FORMAT_R16_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx->DrawIndexed(6, 0, 0);
}
