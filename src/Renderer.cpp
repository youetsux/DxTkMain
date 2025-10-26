#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "Renderer.h"


using namespace DirectX;

Renderer::~Renderer() = default;

void Renderer::Initialize(HWND hwnd, unsigned w, unsigned h)
{
    m_dev = std::make_unique<DX::DeviceResources>();
    m_dev->SetWindow(hwnd, w, h);
    m_dev->CreateDeviceResources();
    m_dev->CreateWindowSizeDependentResources();

    m_states = std::make_unique<CommonStates>(m_dev->Dev());
    m_sprite = std::make_unique<SpriteBatch>(m_dev->Ctx());

    m_quad = std::make_unique<Quad>();
    m_quad->Initialize(m_dev->Dev()); // use device to create resources
}

void Renderer::OnResize(unsigned w, unsigned h)
{
    m_dev->WindowSizeChanged(w, h);
}

// Renderer.cpp
void Renderer::BeginFrame()
{
    auto ctx = m_dev->Ctx();
    auto rtv = m_dev->RTV();
    auto dsv = m_dev->DSV();

    // RTV/DSVをバインド
    ID3D11RenderTargetView* rtvs[] = { rtv };     // ← 配列で渡す
    ctx->OMSetRenderTargets(1, rtvs, dsv);

    // ★ 毎フレーム、確実に VP を設定
    D3D11_VIEWPORT vp{};
    vp.TopLeftX = 0; vp.TopLeftY = 0;
    vp.Width = static_cast<FLOAT>(m_dev->Width());
    vp.Height = static_cast<FLOAT>(m_dev->Height());
    vp.MinDepth = 0; vp.MaxDepth = 1;
    ctx->RSSetViewports(1, &vp);

    // クリア
    ctx->ClearRenderTargetView(rtv, m_clear);
    if (dsv) ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}


void Renderer::EndFrame()
{
    // draw demo quad
    m_quad->Draw(m_dev->Ctx());
}

void Renderer::Present()
{
    m_dev->Present();
}