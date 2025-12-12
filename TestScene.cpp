#include "TestScene.h"
#include "Engine/GameObject.h"
#include "ModelComponent.h"
#include "Engine/EngineTime.h"
#include  "Engine/input.h"

TestScene::TestScene()
{
}

TestScene::~TestScene()
{
    Release();
}

void TestScene::Initialize()
{
    // ルート配下に表示用オブジェクトを生成
    actor_ = new GameObject(&Root(), "TestActor");
    auto* mc = new ModelComponent(actor_, "Assets/Enemy.fbx", 1.8f);

    // 表示位置（必要に応じて調整）
    actor_->GetTransform().position_ = { 0.0f, -1.0f, 0.0f };
    actor_->GetTransform().scale_ = { 1.0f, 1.0f, 1.0f };

    
    // モデル表示コンポーネントを追加
    // ★ここでパスをあなたの環境に合わせてください
    //actor_->AddComponent(new ModelComponent(actor_, "Assets/TriAvater.fbx", 1.8));
    actor_->AddComponent(mc);

    mc->SetAnimStack(0);
    mc->Play(0, 100, 1.0f, true);
}

void TestScene::Update()
{
    if (!actor_) return;

    // Y軸回転（ラジアン）
    actor_->GetTransform().rotate_.y += 30.0f * (float)EngineTime::DeltaTime();

    if (Input::IsKey(VK_LEFT))
    {
		actor_->GetTransform().position_.x -= 2.0f * (float)EngineTime::DeltaTime();
    }
    if (Input::IsKey(VK_RIGHT))
    {
        actor_->GetTransform().position_.x += 2.0f * (float)EngineTime::DeltaTime();
    }
    if(Input::IsKeyUp(VK_SPACE))
    {
		MessageBoxA(nullptr, "Space Key Pressed!", "Info", MB_OK);
	}
}

void TestScene::Draw()
{
}

void TestScene::Release()
{
}
