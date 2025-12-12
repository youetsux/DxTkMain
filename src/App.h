#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <memory>

#include "../Engine/DeviceResources.h"
#include "../Engine/Renderer.h"
#include "../Engine/Camera.h"
#include "../Engine/Gfx.h"

#include "../Engine/SceneManager.h"

//------------------------------------------------------------
// App
// - WinMain から呼ばれるアプリケーション本体
// - SceneManager を回すだけの薄い殻
//------------------------------------------------------------
class App {
public:
    App() = default;
    ~App() { Shutdown(); }

    void Initialize(HWND hwnd, unsigned w, unsigned h);
    void OnResize(unsigned w, unsigned h);

    void Update();
    void Render();
    void Shutdown();

    bool Ready() const { return m_ready; }

private:
    std::shared_ptr<DX::DeviceResources> m_dev;
    Renderer      m_renderer;
    SceneManager  m_sceneManager;

    bool m_ready = false;
};
