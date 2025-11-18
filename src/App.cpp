#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <DirectXMath.h>
#include "Dice.h"
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
    Camera::SetPosition(XMVectorSet(0, 3, -3, 0));
    Camera::SetTarget(XMVectorSet(0, 0, 0, 0));


    //m_quad.LoadTexture(".\\Assets\\Dice.png");
    //m_quad.Initialize();

    m_dice.Initialize();
	m_dice.LoadTexture(L".\\Assets\\Dice.png");


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
    Camera::Update();
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
    XMMATRIX Wy = XMMatrixRotationY(m_angle);
    XMMATRIX Wx = XMMatrixRotationX(m_angle/3.0f);
    XMMATRIX V = Camera::GetViewMatrix();
    XMMATRIX P = Camera::GetProjectionMatrix();
    //XMMATRIX world = XMMatrixIdentity();
    //XMMATRIX view = XMMatrixLookAtLH({ 2,2,-3 }, { 0,0,0 }, { 0,1,0 });
    //float aspect = static_cast<float>(Gfx::Width()) / static_cast<float>(Gfx::Height());
    //XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.1f, 100.0f);
    //XMMATRIX WVP = W * V * P;

    //// 引数なし版を採用しているなら：m_quad.Draw(WVP);
    //m_quad.Draw( WVP);
    m_dice.Draw(Wy, V, P);
    
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
