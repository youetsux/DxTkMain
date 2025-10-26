#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "App.h"

void App::Initialize(HWND hwnd, unsigned w, unsigned h)
{
    m_renderer.Initialize(hwnd, w, h);
}

void App::OnResize(unsigned w, unsigned h)
{
    m_renderer.OnResize(w, h);
}

void App::Update()
{
    // TODO
}

void App::Render()
{
    FrameScope frame(m_renderer);
    // drawing happens inside Renderer::EndFrame (Quad)
    m_renderer.BeginFrame();  // クリア & VP/RTV/DSV
    m_renderer.EndFrame();    // ★ ここで m_quad->Draw() が呼ばれる設計
    m_renderer.Present();     // 表示
}