#include "Quad.h"
#include "Gfx.h"
#include <d3dcompiler.h>
#include <Windows.h> // MultiByteToWideChar 用
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")  // ← これを追加

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace
{
    struct QuadVtx { float x, y, z; float u, v; };
    constexpr const char* kShaderPath = "assets/shaders/Quad.hlsl";
    constexpr const char* kTexturePath = ".\\Assets\\texture.png";
}

// UTF-8 → UTF-16 変換（Quad 内部用）
static std::wstring ToWString(const std::string& s)
{
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), len);
    return w;
}

HRESULT Quad::Initialize()
{
    auto* device = Gfx::Dev();
    if (!device) return E_POINTER;

    QuadVtx vertices[] = {
        { -0.5f,  0.5f, 0.f, 0.f, 0.f },
        {  0.5f,  0.5f, 0.f, 1.f, 0.f },
        {  0.5f, -0.5f, 0.f, 1.f, 1.f },
        { -0.5f, -0.5f, 0.f, 0.f, 1.f },
    };
    uint16_t indices[] = { 0,1,2, 0,2,3 };

	D3D11_BUFFER_DESC vbDesc{};// 頂点バッファ
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    D3D11_SUBRESOURCE_DATA vbData{ vertices };
    HRESULT hr = device->CreateBuffer(&vbDesc, &vbData, m_vb.GetAddressOf());
    if (FAILED(hr)) return hr;

	D3D11_BUFFER_DESC ibDesc{};// インデックスバッファ
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.ByteWidth = sizeof(indices);
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    D3D11_SUBRESOURCE_DATA ibData{ indices };
    hr = device->CreateBuffer(&ibDesc, &ibData, m_ib.GetAddressOf());
    if (FAILED(hr)) return hr;

    // ---------- シェーダコンパイル ----------
    std::wstring wShaderPath = ToWString(kShaderPath);

    ComPtr<ID3DBlob> vsBlob, psBlob, err;
    hr = D3DCompileFromFile(wShaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "VS", "vs_5_0", 0, 0, vsBlob.GetAddressOf(), err.GetAddressOf());
    if (FAILED(hr)) { if (err) OutputDebugStringA((char*)err->GetBufferPointer()); return hr; }

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        nullptr, m_vs.GetAddressOf());
    if (FAILED(hr)) return hr;

    err.Reset();
    hr = D3DCompileFromFile(wShaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "PS", "ps_5_0", 0, 0, psBlob.GetAddressOf(), err.GetAddressOf());
    if (FAILED(hr)) { if (err) OutputDebugStringA((char*)err->GetBufferPointer()); return hr; }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
        nullptr, m_ps.GetAddressOf());
    if (FAILED(hr)) return hr;

	// ---------- 入力レイアウト & 定数バッファ ----------
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, sizeof(float) * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = device->CreateInputLayout(layout, _countof(layout),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        m_layout.GetAddressOf());
    if (FAILED(hr)) return hr;

	// 定数バッファ
    D3D11_BUFFER_DESC cbd{};
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.ByteWidth = sizeof(XMMATRIX);
    cbd.Usage = D3D11_USAGE_DEFAULT;
    hr = device->CreateBuffer(&cbd, nullptr, m_cbPerObject.GetAddressOf());
    if (FAILED(hr)) return hr;

    // ---------- テクスチャ読み込み ----------
    hr = m_texture.Load(kTexturePath);
    return hr;
}

void Quad::Draw(const DirectX::XMMATRIX& wvp)
{
    auto* ctx = Gfx::Ctx();
    if (!ctx) return;

    UINT stride = sizeof(QuadVtx), offset = 0;
    ID3D11Buffer* vb = m_vb.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(m_ib.Get(), DXGI_FORMAT_R16_UINT, 0);
    ctx->IASetInputLayout(m_layout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx->VSSetShader(m_vs.Get(), nullptr, 0);
    ctx->PSSetShader(m_ps.Get(), nullptr, 0);

    XMMATRIX wvpT = XMMatrixTranspose(wvp);
    ctx->UpdateSubresource(m_cbPerObject.Get(), 0, nullptr, &wvpT, 0, 0);

    ID3D11Buffer* cbs[] = { m_cbPerObject.Get() };
    ctx->VSSetConstantBuffers(0, 1, cbs);
    ctx->PSSetConstantBuffers(0, 1, cbs);

    if (m_states && m_texture.GetSRV())
    {
        ID3D11ShaderResourceView* srv = m_texture.GetSRV();
        ID3D11SamplerState* samp = m_states->LinearWrap();
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->PSSetSamplers(0, 1, &samp);
    }
    ctx->OMSetBlendState(m_states->NonPremultiplied(), nullptr, 0xFFFFFFFF); // ← 重要

    ctx->DrawIndexed(6, 0, 0);
    ctx->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
}
