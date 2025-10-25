#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "Renderer.h"

void Renderer::Initialize(HWND hwnd, unsigned w, unsigned h)
{
    m_dev = std::make_unique<DX::DeviceResources>();
    m_dev->SetWindow(hwnd, w, h);
    m_dev->CreateDeviceResources();
    m_dev->CreateWindowSizeDependentResources();

    m_states = std::make_unique<DirectX::CommonStates>(m_dev->Dev());
    m_sprite = std::make_unique<DirectX::SpriteBatch>(m_dev->Ctx());
    // m_font   = std::make_unique<DirectX::SpriteFont>(m_dev->Dev(), L"assets/ui/UIFont.spritefont");
}

void Renderer::OnResize(unsigned w, unsigned h)
{
    m_dev->WindowSizeChanged(w, h);
}

void Renderer::BeginFrame()
{
    auto ctx = m_dev->Ctx();
    auto rtv = m_dev->RTV();
    auto dsv = m_dev->DSV();
    ctx->OMSetRenderTargets(1, &rtv, dsv);
    ctx->ClearRenderTargetView(rtv, m_clear);
    if (dsv) ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void Renderer::EndFrame()
{
    // post-frame hook
}

void Renderer::Present()
{
    m_dev->Present();
}
