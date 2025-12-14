#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "App.h"
#include "../Engine/EngineTime.h"
#include "../Engine/Model.h"
#include "../Engine/Input.h"
#include "../Engine/Sound.h"

//------------------------------------------------------------
// 初期化
//------------------------------------------------------------
void App::Initialize(HWND hwnd, unsigned w, unsigned h)
{
    // DeviceResources
    m_dev = std::make_shared<DX::DeviceResources>();
    m_dev->SetWindow(hwnd, w, h);
    m_dev->CreateDeviceResources();
    m_dev->CreateWindowSizeDependentResources();

    // Gfx
    Gfx::Init(m_dev.get());

    // Renderer
    m_renderer.Initialize();

    // Camera
    Camera::Initialize();
    Camera::SetPerspective(DirectX::XMConvertToRadians(60.0f),
        float(w) / float(h));
    Camera::SetPosition({ 0.0f, 2.0f, -5.0f, 0.0f });
    Camera::SetTarget({ 0.0f, 0.0f,  0.0f, 0.0f });

    // Model system
    Model::Initialize();

	// Input system
	Input::Initialize(hwnd);

	Sound::Initialize();

    // ★ SceneManager 初期化（TestScene が作られる）
    m_sceneManager.Initialize();

    m_ready = true;
}

//------------------------------------------------------------
// リサイズ
//------------------------------------------------------------
void App::OnResize(unsigned w, unsigned h)
{
    if (!m_ready || w == 0 || h == 0) return;

    m_dev->WindowSizeChanged(w, h);
    Camera::OnResize(w, h);
}

//------------------------------------------------------------
// 更新
//------------------------------------------------------------
void App::Update()
{
    if (!m_ready) return;
    Input::Update(); // Input 更新
	Sound::Update(); // Sound 更新
	
	m_sceneManager.Update(); // ★ Scene 駆動更新

    Camera::Update(); // カメラ更新
}

//------------------------------------------------------------
// 描画
//------------------------------------------------------------
//------------------------------------------------------------
// 描画
//------------------------------------------------------------
void App::Render()
{
    if (!m_ready) return;

    m_renderer.BeginFrame();

    //========================================================
    // 旧：即時描画（現状維持・デフォルト）
    //========================================================
    m_sceneManager.Draw();

    //========================================================
    // 新：Submit → Execute 経路（※まだ未使用）
    //========================================================
    // m_sceneManager.SubmitDraw(m_renderer);
    // m_renderer.Execute();

    m_renderer.EndFrame();
    m_renderer.Present();
}


//------------------------------------------------------------
// 終了
//------------------------------------------------------------
void App::Shutdown()
{
    if (!m_ready) return;

    m_sceneManager.Release();
    Sound::Shutdown();
    Gfx::Reset();
    m_renderer = Renderer{};
    m_ready = false;
}
