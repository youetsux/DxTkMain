#include "Renderer.h"
#include "Camera.h"  // 必要なら

using namespace DirectX;

void Renderer::Initialize()
{
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

    // ★変更：キューをフレーム境界で初期化（未使用でも既存動作に影響なし）
    m_queue3D.clear();
    m_has2D = false;
    m_hasUI = false;
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

//============================================================
// ★変更：Submit（フェーズ1）
//============================================================
void Renderer::SubmitModel3D(int modelHandle, bool drawSkeleton)
{
    if (modelHandle < 0) return;

    ModelDrawCmd cmd;
    cmd.handle = modelHandle;
    cmd.drawSkeleton = drawSkeleton;
    m_queue3D.push_back(cmd);
}

void Renderer::Submit2D()
{
    // placeholder（後で2Dコマンド構造体を入れる）
    m_has2D = true;
}

void Renderer::SubmitUI()
{
    // placeholder（後でUIコマンド構造体を入れる）
    m_hasUI = true;
}

//============================================================
// ★変更：Execute（3D→2D→UI）
//  - 今は App/Scene 側が呼ばない想定なので既存動作は変わらない
//============================================================
void Renderer::Execute()
{
    // 3D
    if (!m_queue3D.empty())
    {
        for (size_t i = 0; i < m_queue3D.size(); ++i)
        {
            const auto& cmd = m_queue3D[i];
            Model::Draw(cmd.handle); // Model側が view/proj を Camera から取って描画 :contentReference[oaicite:3]{index=3}
            if (cmd.drawSkeleton)
            {
                Model::DrawSkeleton(cmd.handle);
            }
        }
    }

    // 2D（未実装：箱）
    if (m_has2D)
    {
        // TODO
    }

    // UI（未実装：箱）
    if (m_hasUI)
    {
        // TODO
    }
}
