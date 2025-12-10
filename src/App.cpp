#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <DirectXMath.h>

#include "App.h"
#include "Model.h"
#include "Transform.h"
#include "EngineTime.h"
#include "Gfx.h"
#include "Camera.h"

#include <vector>

#include <GeometricPrimitive.h>

using namespace DirectX;

// ------------------------------------------------------------
// App.cpp 内だけで使うグローバル
// ------------------------------------------------------------
namespace
{
    // 並べるモデル数（必要に応じて変えてください）
    constexpr int MODEL_NUM = 1;

    // SillyDancing 用のモデルハンドル配列
    int g_hSilly[MODEL_NUM];

    // 各モデルの Transform
    Transform g_sillyTransform[MODEL_NUM];

    // 他のモデル（必要なら残す）
    int g_hEnemy = -1;
    int g_hTriAvatar = -1;
	int h_gOtherModel = -1;

    std::unique_ptr<DirectX::GeometricPrimitive> g_unitCube;

    void InitDebugPrimitives()
    {
        auto device = Gfx::Ctx();
        g_unitCube = DirectX::GeometricPrimitive::CreateCube(device);
    }
    void DrawUnitHeightBox()
    {
        using namespace DirectX;

        XMMATRIX world =
            XMMatrixTranslation(-1.0f, 0.5f, 0.0f); // 高さ1、足元0

        auto view = Camera::GetViewMatrix();
        auto proj = Camera::GetProjectionMatrix();

        g_unitCube->Draw(world, view, proj, Colors::Red);
    }
}



// ------------------------------------------------------------
// 初期化
// ------------------------------------------------------------
void App::Initialize(HWND hwnd, unsigned w, unsigned h)
{
    // 1) DeviceResources を App が作って所有
    m_dev = std::make_shared<DX::DeviceResources>();
    m_dev->SetWindow(hwnd, w, h);
    m_dev->CreateDeviceResources();
    m_dev->CreateWindowSizeDependentResources();

    // 2) Gfx に登録
    Gfx::Init(m_dev.get());

    // 3) Renderer 初期化
    m_renderer.Initialize();

    // 4) カメラ初期化
    Camera::Initialize();
    Camera::SetPerspective(XMConvertToRadians(45.0f), float(w) / float(h));
    Camera::SetPosition(XMVectorSet(0, 0.9, -5, 0));
    Camera::SetTarget(XMVectorSet(0, 0.9, 0, 0));

    // モデル管理初期化
    Model::Initialize();

    // SillyDancing を MODEL_NUM 体ロード（中身は共有される）
    for (int i = 0; i < MODEL_NUM; ++i)
    {
        g_hSilly[i] = Model::Load(".\\Assets\\SillyDancing.fbx", 1.0f);
        // 2つ目の AnimStack を使うならそのまま
        Model::SetAnimStack(g_hSilly[i], 1);
        // 0〜229 フレームを 1.0 倍速でループ
        Model::SetAnimFrame(g_hSilly[i], 0, 229, 1.0f);
    }

    // 横一列に並べる Transform を設定
    {
        // 中央基準に左右へ等間隔に並べる
        const float spacing = 0.5f;      // モデル間の間隔
        const float baseZ = 3.0f;      // 手前/奥の位置
        const float scale = 1.0f;

        const float centerIndex = (MODEL_NUM - 1) * 0.5f;

        for (int i = 0; i < MODEL_NUM; ++i)
        {
            float offset = float(i) - centerIndex; // -...0...+
            float x = offset * spacing;

            g_sillyTransform[i].position_ = { x, 0.0f, baseZ };
            g_sillyTransform[i].rotate_ = { 0.0f, 0.0f, 0.0f };
            g_sillyTransform[i].scale_ = { scale, scale, scale };
        }
    }

    // 他モデルが必要ならここでロード
    g_hEnemy = Model::Load(".\\Assets\\Enemy.fbx", 1.0);
    Model::SetAnimFrame(g_hEnemy, 0, 100, 1.0f);
    Model::SetAnimLoop(g_hEnemy, false);
    

    h_gOtherModel = Model::Load(".\\Assets\\GS_MotionSet.fbx", 2.0);
    Model::SetAnimFrame(h_gOtherModel, 0, 2475, 1.0f);
    Model::SetAnimLoop(h_gOtherModel, true);

    g_hTriAvatar = Model::Load(".\\Assets\\TriAvater.fbx", 1.5f);
   //g_hTriAvatar = Model::Load(".\\Assets\\tri2.fbx", 1.0f);
    // TriAvatar は今回は静的でもよいなら SetAnimFrame は省略可

    InitDebugPrimitives();
    m_ready = true;
}

// ------------------------------------------------------------
// リサイズ
// ------------------------------------------------------------
void App::OnResize(unsigned w, unsigned h)
{
    if (!m_ready || w == 0 || h == 0) return;

    m_dev->WindowSizeChanged(w, h);
    Camera::OnResize(w, h);
}

// ------------------------------------------------------------
// 更新
// ------------------------------------------------------------
void App::Update()
{
    if (!m_ready) return;

    // スペースキーでアニメのポーズをトグル
    {
        static bool prevSpaceDown = false;
        SHORT state = GetAsyncKeyState(VK_SPACE);
        bool  currSpaceDown = (state & 0x8000) != 0;

        // 立ち上がり検出（前フレームは離していて、今フレーム押された）
        if (currSpaceDown && !prevSpaceDown)
        {
            // ここでは例として hModel, hModel2, hModel3 をまとめてトグル
            // 1体だけならそのハンドルだけでOK
			for (int i = 0;i < MODEL_NUM;++i) {
				int hModel = g_hSilly[i];
				bool paused = Model::IsAnimPaused(hModel);
				bool next = !paused;
				Model::SetAnimPaused(hModel, next);
			}
			bool paused = Model::IsAnimPaused(g_hEnemy);
			bool next = !paused;
            Model::SetAnimPaused(g_hEnemy, next);
            Model::SetAnimPaused(g_hTriAvatar, next);
        }

        prevSpaceDown = currSpaceDown;
    }

    // EngineTime::Tick() は WinMain 側で呼んでいる前提
    Camera::Update();

    // 回転させたい場合はここで Transform をいじる
    // 例：全員を少しずつ Y 回転させる
     //const float rotSpeed = XMConvertToRadians(1.0f); // 10°/秒
     //float dt = (float)EngineTime::DeltaTime();
     //for (int i = 0; i < MODEL_NUM; ++i) {
     //    g_sillyTransform[i].rotate_.y += rotSpeed * dt;
     //}
}

// ------------------------------------------------------------
// 描画
// ------------------------------------------------------------
void App::Render()
{
    if (!m_ready) return;

    m_renderer.BeginFrame();

    // カメラ行列（今は Model::Draw の中で Camera 取得しているので、
    // ここで V/P を使わなくてもよい）
    XMMATRIX V = Camera::GetViewMatrix();
    XMMATRIX P = Camera::GetProjectionMatrix();


    // SillyDancing を横一列に描画
    
    //for (int i = 0; i < MODEL_NUM; ++i)
    //{
    //    // 毎フレームワールド行列を更新
    //    //g_sillyTransform[i].Calclation();

    //   // Model::SetTransform(g_hSilly[i], g_sillyTransform[i]);
       Transform temp;
       temp.position_.x = 1.0f;
        Model::SetTransform(g_hSilly[0], temp);
        Model::Draw(g_hSilly[0]);
    //    // スケルトンを重ねて描きたい場合は:
        // Model::DrawSkeleton(g_hSilly[i]);
    //}

    // ついでに他モデルも描画したければここで
    // （位置は適当に）
    {
        static Transform tEnemy;
        tEnemy.position_ = { 2.0f, 0.0f, 0.0f };
        tEnemy.scale_ = { 1.0f, 1.0f, 1.0f };
        tEnemy.Calclation();

        Model::SetTransform(g_hEnemy, tEnemy);
        Model::Draw(g_hEnemy);

        static Transform tri;
        tri.position_ = { 0.0f, 0.0f, 0.0f };
        tri.scale_ = { 1.0f, 1.0f, 1.0f };
		tri.rotate_ = { 0.0f, 0.0f, 0.0f };
        //tri.rotate_.y += 1;
        tri.Calclation();  
        Model::SetTransform(g_hTriAvatar, tri);
        Model::Draw(g_hTriAvatar);

        static Transform ken;
        ken.position_ = { -2.0f, 0.0f, 0.0f };
        ken.scale_ = { 1.0f, 1.0f, 1.0f };
        ken.rotate_ = { 0.0f, 0.0f, 0.0f };
		ken.Calclation();
        Model::SetTransform(h_gOtherModel, ken);
		Model::Draw(h_gOtherModel);



    }

    // 単位ボックス
    DrawUnitHeightBox();

    m_renderer.EndFrame();
    m_renderer.Present();
}

// ------------------------------------------------------------
// 終了
// ------------------------------------------------------------
void App::Shutdown()
{
    if (!m_ready) return;

    Gfx::Reset();
    m_renderer = Renderer{};
    m_ready = false;
}
