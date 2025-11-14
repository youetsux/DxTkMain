#include "Renderer.h"
#include "Camera.h"

using namespace DirectX;
namespace DXTK = DirectX::DX11;

void Renderer::Initialize()
{
    // ★ App 側で先に Gfx::Init() 済みであること
    if (!Gfx::IsReady()) return;

    auto* dev = Gfx::Dev();

    // 共通ステートオブジェクト
    m_states = std::make_unique<CommonStates>(dev);

    // 共通 BasicEffect を 1 つだけ作成
    m_basicFx = std::make_unique<DXTK::BasicEffect>(dev);

    // 必要最低限のデフォルト設定（詳細なライティング設定は各描画側で上書きしてもよい）
    m_basicFx->SetLightingEnabled(true);
    m_basicFx->SetPerPixelLighting(true);
    m_basicFx->SetVertexColorEnabled(false);
}

void Renderer::OnResize(unsigned w, unsigned h)
{
    if (!Gfx::IsReady() || w == 0 || h == 0) return;

    // RTV/DSV などのリサイズを更新
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
        ctx->ClearDepthStencilView(dsv,
            D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // デフォルトステート設定（不透明 3D 用）
    if (m_states)
    {
        ctx->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
        ctx->OMSetDepthStencilState(m_states->DepthDefault(), 0);
        ctx->RSSetState(m_states->CullNone());
    }
}

void Renderer::EndFrame()
{
    // ここでは特に何もしない。
    // 将来的にポストエフェクトなどを入れるならここで実行。
}

void Renderer::Present()
{
    if (!Gfx::IsReady()) return;
    if (auto dr = Gfx::Raw())
        dr->Present();
}
