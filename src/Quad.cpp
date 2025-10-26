#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3dcompiler.h>
#include <stdexcept>
#include "Quad.h"

using namespace DirectX;
static void ThrowIfFailed(HRESULT hr){ if(FAILED(hr)) throw std::runtime_error("HRESULT failed"); }

void Quad::Initialize(ID3D11Device* device)
{
    Microsoft::WRL::ComPtr<ID3DBlob> vsb, psb, err;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3D10_SHADER_DEBUG;
#endif
    ThrowIfFailed(D3DCompileFromFile(L"assets/shaders/Quad.hlsl", nullptr, nullptr, "VS", "vs_5_0", flags, 0, vsb.GetAddressOf(), err.GetAddressOf()));
    err.Reset();
    ThrowIfFailed(D3DCompileFromFile(L"assets/shaders/Quad.hlsl", nullptr, nullptr, "PS", "ps_5_0", flags, 0, psb.GetAddressOf(), err.GetAddressOf()));

    ThrowIfFailed(device->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, m_vs.GetAddressOf()));
    ThrowIfFailed(device->CreatePixelShader (psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, m_ps.GetAddressOf()));

    D3D11_INPUT_ELEMENT_DESC ildesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    ThrowIfFailed(device->CreateInputLayout(ildesc, _countof(ildesc), vsb->GetBufferPointer(), vsb->GetBufferSize(), m_layout.GetAddressOf()));

    XMFLOAT3 v[4] = {
        {-0.5f,  0.5f, 0.0f},
        { 0.5f,  0.5f, 0.0f},
        { 0.5f, -0.5f, 0.0f},
        {-0.5f, -0.5f, 0.0f},
    };
    D3D11_BUFFER_DESC vbd{}; vbd.ByteWidth = sizeof(v); vbd.Usage = D3D11_USAGE_DEFAULT; vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vinit{ v, 0, 0 };
    ThrowIfFailed(device->CreateBuffer(&vbd, &vinit, m_vb.GetAddressOf()));

    unsigned short idx[6] = { 0,1,2, 0,2,3 };
    D3D11_BUFFER_DESC ibd{}; ibd.ByteWidth = sizeof(idx); ibd.Usage = D3D11_USAGE_DEFAULT; ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA iinit{ idx, 0, 0 };
    ThrowIfFailed(device->CreateBuffer(&ibd, &iinit, m_ib.GetAddressOf()));

    D3D11_BUFFER_DESC cbd{}; cbd.ByteWidth = sizeof(CBColor); cbd.Usage = D3D11_USAGE_DEFAULT; cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ThrowIfFailed(device->CreateBuffer(&cbd, nullptr, m_cb.GetAddressOf()));

    // CullNone Rasterizer
    D3D11_RASTERIZER_DESC rs{};
    rs.FillMode = D3D11_FILL_SOLID;
    rs.CullMode = D3D11_CULL_NONE;
    rs.DepthClipEnable = TRUE;
    ThrowIfFailed(device->CreateRasterizerState(&rs, m_rsCullNone.GetAddressOf()));

    // DepthStencil OFF
    D3D11_DEPTH_STENCIL_DESC ds{};
    ds.DepthEnable = FALSE;
    ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    ThrowIfFailed(device->CreateDepthStencilState(&ds, m_dsOff.GetAddressOf()));
}

void Quad::Draw(ID3D11DeviceContext* ctx)
{
    ctx->RSSetState(m_rsCullNone.Get());
    ctx->OMSetDepthStencilState(m_dsOff.Get(), 0);

    UINT stride = sizeof(XMFLOAT3), offset = 0;
    ID3D11Buffer* vb = m_vb.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(m_ib.Get(), DXGI_FORMAT_R16_UINT, 0);
    ctx->IASetInputLayout(m_layout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(m_vs.Get(), nullptr, 0);
    ctx->PSSetShader(m_ps.Get(), nullptr, 0);

    CBColor c{ XMFLOAT4(0, 1, 0, 1) }; // vivid green
    ctx->UpdateSubresource(m_cb.Get(), 0, nullptr, &c, 0, 0);
    ID3D11Buffer* cbs[] = { m_cb.Get() };
    ctx->VSSetConstantBuffers(0, 1, cbs);
    ctx->PSSetConstantBuffers(0, 1, cbs);

    ctx->DrawIndexed(6, 0, 0);
}

void Quad::Release()
{
    m_cb.Reset(); m_ib.Reset(); m_vb.Reset();
    m_layout.Reset(); m_ps.Reset(); m_vs.Reset();
    m_rsCullNone.Reset(); m_dsOff.Reset();
}