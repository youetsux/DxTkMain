#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <memory>

#include "DeviceResources.h"   // DX::DeviceResources
#include "Renderer.h"          // Begin/End/Present を持つ
//#include "Quad.h"              // Quad::Initialize/Draw
#include "Camera.h"            // namespace Camera
#include "Gfx.h"    
#include "Dice.h"

class App {
public:
    App() = default;
    ~App() { Shutdown(); }

    void Initialize(HWND hwnd, unsigned w, unsigned h);
    void OnResize(unsigned w, unsigned h);

    // 毎フレーム
    void Update();
    void Render();

    // 終了時（明示呼び出し or デストラクタ経由）
    void Shutdown();

    bool Ready() const { return m_ready; }

private:
    // ★ App が所有（ここが“強参照”）
    std::shared_ptr<DX::DeviceResources> m_dev;

    Renderer m_renderer;
    //Quad     m_quad;
	Dice    m_dice;

    bool m_ready = false;

    // デモ用：回転角（App 管理でも Renderer 管理でも可）
    float m_angle = 0.0f;
};
