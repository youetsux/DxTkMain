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
}

void App::Render()
{
    FrameScope frame(m_renderer);

    // --- 3D draw via m_renderer.Dev()/Ctx() ---

    // --- 2D ---
    // if (auto sb = m_renderer.Sprite()) {
    //     sb->Begin();
    //     if (auto font = m_renderer.Font()) {
    //         font->DrawString(sb, L"Hello DirectXTK", DirectX::XMFLOAT2{20,20});
    //     }
    //     sb->End();
    // }

    m_renderer.Present();
}
