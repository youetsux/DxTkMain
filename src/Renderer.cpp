#include "Renderer.h"
#include "Camera.h"  // 必要なら

using namespace DirectX;

void Renderer::Initialize()
{
    // ★ Appが先にGfx::Init()を済ませている前提
    m_states = std::make_unique<CommonStates>(Gfx::Dev());

}

void Renderer::OnResize(unsigned w, unsigned h)
{
    if (!Gfx::IsReady() || w == 0 || h == 0) return;

    // RTV/DSVなどのリサイズを更新
    Gfx::WindowSizeChanged(w, h);

    // カメラのアスペクト比更新
    Camera::OnResize(w, h);
}

void Renderer::BeginFrame()
{
    if (!Gfx::IsReady()) return;

    auto ctx = Gfx::Ctx();
    auto rtv = Gfx::RTV();
    auto dsv = Gfx::DSV();
    if (!rtv) return;

    ID3D11RenderTargetView* rtvs[] = { rtv };
    ctx->OMSetRenderTargets(1, rtvs, dsv);

    D3D11_VIEWPORT vp{};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = static_cast<FLOAT>(Gfx::Width());
    vp.Height = static_cast<FLOAT>(Gfx::Height());
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);

    // 画面クリア
    ctx->ClearRenderTargetView(rtv, m_clear);
    if (dsv)
        ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // デフォルトステート設定（不透明3D用）
    ctx->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(m_states->DepthDefault(), 0);
    ctx->RSSetState(m_states->CullNone());
}

void Renderer::EndFrame()
{
    // ここで特に解除は不要
    // 将来的にポストエフェクトなどを挟む場合に使う
}

void Renderer::Present()
{
    if (!Gfx::IsReady()) return;
    if (auto dr = Gfx::Raw())
        dr->Present();
}
