#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <DirectXMath.h>

#include "App.h"

using namespace DirectX;

void App::Initialize(HWND hwnd, unsigned w, unsigned h)
{
    // 1) DeviceResources を App が作って所有
    m_dev = std::make_shared<DX::DeviceResources>();
    m_dev->SetWindow(hwnd, w, h);
    m_dev->CreateDeviceResources();
    m_dev->CreateWindowSizeDependentResources();

    // 2) Gfx に登録（以降 Dev/Ctx/RTV/DSV/Width/Height がどこからでも使える）
    Gfx::Init(m_dev.get());

    // 3) Renderer 初期化（デバイス非所有）
    m_renderer.Initialize();

    // 4) カメラ初期化（ページ準拠API）
    Camera::Initialize();
    Camera::SetPerspective(XM_PIDIV4, float(w) / float(h));
    Camera::SetPosition(XMVectorSet(0, 0, -3, 0));
    Camera::SetTarget(XMVectorSet(0, 0, 0, 0));

    // 5) Quad 初期化（どちらでもOK）
    //   A) 引数なし版：Quad::Initialize() 内で Gfx::Dev() を取得
    // m_quad.Initialize();
    //   B) 引数あり版：今の定義が device を要するならこちら
    m_quad.Initialize();

    m_ready = true;
}

void App::OnResize(unsigned w, unsigned h)
{
    if (!m_ready || w == 0 || h == 0) return;

    // RTV/DSV等のサイズ依存リソースを更新
    m_dev->WindowSizeChanged(w, h);

    // カメラのアスペクト更新（使っていれば）
    Camera::OnResize(w, h);
}

void App::Update()
{
	static float dt = 1.0f / 60.0f; // 仮固定値（本来は経過時間を計測）
    if (!m_ready) return;

    // デモ用途：回転角を更新（必要なければ削除OK）
    m_angle += dt * XM_PIDIV4; // 45°/s
}

void App::Render()
{
    if (!m_ready) return;

    m_renderer.BeginFrame();

    // WVP を App 側で合成して Quad に渡す（Quad がパイプラインをバインド）
    XMMATRIX W = XMMatrixRotationZ(m_angle);
    XMMATRIX V = Camera::GetViewMatrix();
    XMMATRIX P = Camera::GetProjectionMatrix();
    XMMATRIX WVP = W * V * P;

    // 引数なし版を採用しているなら：m_quad.Draw(WVP);
    m_quad.Draw( WVP);

    m_renderer.EndFrame();
    m_renderer.Present();
}

void App::Shutdown()
{
    if (!m_ready) return;

    // 先に Gfx を外す（ダングリング防止）
    Gfx::Reset();

    // 所有リソースを破棄
	m_renderer = Renderer{};

    m_ready = false;
}
