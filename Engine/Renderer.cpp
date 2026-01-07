#include "Renderer.h"
#include "Camera.h"

using namespace DirectX;

void Renderer::Initialize()
{
}

void Renderer::OnResize(unsigned w, unsigned h)
{
    if (!Gfx::IsReady() || w == 0 || h == 0) return;


    Gfx::WindowSizeChanged(w, h);


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


    ctx->ClearRenderTargetView(rtv, m_clear);
    if (dsv)
        ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);


    m_queue3D.clear();
    m_has2D = false;
    m_hasUI = false;
}

void Renderer::EndFrame()
{


}

void Renderer::Present()
{
    if (!Gfx::IsReady()) return;
    if (auto dr = Gfx::Raw())
        dr->Present();
}

//============================================================

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

    m_has2D = true;
}

void Renderer::SubmitUI()
{

    m_hasUI = true;
}

//============================================================


//============================================================
void Renderer::Execute()
{
    // 3D
    if (!m_queue3D.empty())
    {
        for (size_t i = 0; i < m_queue3D.size(); ++i)
        {
            const auto& cmd = m_queue3D[i];
            Model::Draw(cmd.handle);
            if (cmd.drawSkeleton)
            {
                Model::DrawSkeleton(cmd.handle);
            }
        }
    }


    if (m_has2D)
    {
        // TODO
    }


    if (m_hasUI)
    {
        // TODO
    }
}
