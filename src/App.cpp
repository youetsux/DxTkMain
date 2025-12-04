#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <DirectXMath.h>

#include "App.h"
#include "Model.h"
#include <chrono>
//#include "FbxModel.h"
#include "Transform.h"
#include <vector>

using namespace DirectX;



// ------------------------------------------------------------
// フレームタイマー（App.cpp 内だけで使う）
// ------------------------------------------------------------
namespace
{
	using Clock = std::chrono::steady_clock;

	Clock::time_point g_startTime;
	Clock::time_point g_prevTime;
	double            g_totalTime = 0.0;  // 起動からの経過時間 [秒]
	double            g_deltaTime = 0.0;  // 直近フレームの経過時間 [秒]

	// タイマー初期化（起動 / リサイズ時など）
	void ResetTimer()
	{
		g_startTime = g_prevTime = Clock::now();
		g_totalTime = 0.0;
		g_deltaTime = 0.0;
	}

	// 1 フレームごとに呼んで、delta / total を更新
	void TickTimer()
	{
		const Clock::time_point now = Clock::now();

		g_deltaTime = std::chrono::duration<double>(now - g_prevTime).count();
		g_totalTime = std::chrono::duration<double>(now - g_startTime).count();

		g_prevTime = now;

		// 安全用：極端に大きな delta が出たときは上限をかけてもよい
		// if (g_deltaTime > 0.1) g_deltaTime = 0.1; // 例: 最大 0.1 秒(=10fps)まで
	}

	// 必要なら App.cpp 内から参照しやすいように getter も用意
	double GetDeltaTime() { return g_deltaTime; }
	double GetTotalTime() { return g_totalTime; }


	int hModel = -1;
	int hModel2 = -1;
	int hModel3 = -1;
}


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
	Camera::SetPerspective(XMConvertToRadians(40.0f), float(w) / float(h));
	Camera::SetPosition(XMVectorSet(0, 150, -300, 0));
	Camera::SetTarget(XMVectorSet(0, 0, 150, 0));

	Model::Initialize();
	
	hModel = Model::Load(".\\Assets\\SillyDancing.fbx");//2475
	Model::SetAnimStack(hModel, 1);
	Model::SetAnimFrame(hModel, 0, 229, 1.0);

	hModel2 = Model::Load(".\\Assets\\Enemy.fbx");
	Model::SetAnimFrame(hModel2, 0, 100, 1.0);
	hModel3 = Model::Load(".\\Assets\\TriAvater.fbx");//2475

	m_ready = true;
	ResetTimer();
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
	//m_angle += dt * 10.0f; // 45°/s
}

void App::Render()
{
	if (!m_ready) return;
	// ★1フレーム分の経過時間を更新
	TickTimer();

	m_renderer.BeginFrame();

	// WVP を App 側で合成して Quad に渡す（Quad がパイプラインをバインド）
	//XMMATRIX Wy = XMMatrixRotationY(m_angle);
	//XMMATRIX Wx = XMMatrixRotationX(m_angle/3.0f);
	XMMATRIX V = Camera::GetViewMatrix();
	XMMATRIX P = Camera::GetProjectionMatrix();


	Transform t;
	t.position_ = { 50, 0, 20};
	t.rotate_ = { 0, 0, 0 };
	t.scale_ = { 0.5f,0.5f, 0.5f };
	t.Calclation();

	static Transform t2;
	t2.position_ = { 0, 0, 0 };
	t2.rotate_.y = t2.rotate_.y + 1.0f;
	t2.scale_ = { 10,10, 10 };
	t2.Calclation();

	static Transform t3;
	t3.position_ = { 50, 0, 50 };
	t3.rotate_.y += 5.0f;
	t3.scale_ = { 0.7f, 0.7f, 0.7f };
	t3.Calclation();

	Model::SetTransform(hModel3, t3);
	Model::Draw(hModel3);
	
	Model::SetTransform(hModel, t);
	Model::Draw(hModel);
	Model::DrawSkeleton(hModel);

	Model::SetTransform(hModel2, t2);
	Model::Draw(hModel2);



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
