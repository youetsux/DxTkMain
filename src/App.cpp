#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "App.h"
#include "camera.h"

using namespace DirectX;

void App::Initialize(HWND hwnd, unsigned w, unsigned h)
{
    m_renderer.Initialize(hwnd, w, h);
    Camera::Initialize();
    Camera::SetPerspective(XM_PIDIV4, float(w) / float(h));
    Camera::SetPosition(XMVectorSet(0, 0, -3, 0));
    Camera::SetTarget(XMVectorSet(0, 0, 0, 0));
}

void App::OnResize(unsigned w, unsigned h)
{
    m_renderer.OnResize(w, h);
    Camera::OnResize(w, h); // aspect更新
}

void App::Update()
{
    // TODO
    Camera::Update();
}

void App::Render()
{
    FrameScope frame(m_renderer);
    // drawing happens inside Renderer::EndFrame (Quad)
    m_renderer.BeginFrame();  // クリア & VP/RTV/DSV
    m_renderer.EndFrame();    // ★ ここで m_quad->Draw() が呼ばれる設計
    m_renderer.Present();     // 表示
}