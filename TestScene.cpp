#include "TestScene.h"

#include "Engine/GameObject.h"
#include "ModelComponent.h"
#include "Engine/Input.h"
#include "Engine/Camera.h"
#include "Engine/EngineTime.h"
#include "Engine/sound.h"

#include <DirectXMath.h>

TestScene::TestScene()
{
}

TestScene::~TestScene()
{
    Release();
}

void TestScene::Initialize()
{
    actor_ = new GameObject(&Root(), "Actor");

    // モデル（必要に応じてパス・正規化高さを調整）
    actor_->AddComponent(new ModelComponent(actor_, "Assets/ShachikuChan.fbx", 2.5f));

    // 初期位置
    actor_->GetTransform().position_ = { 0.0f, 0.0f, 0.0f };
}

void TestScene::Update()
{
    if (!actor_) return;

    const float dt = (float)EngineTime::DeltaTime();

    // --- 入力で左右に動かす（追従が分かりやすいので一緒に） ---
    if (Input::IsKey(VK_LEFT))
    {
        actor_->GetTransform().position_.x -= 2.0f * dt;
    }
    if (Input::IsKey(VK_RIGHT))
    {
        actor_->GetTransform().position_.x += 2.0f * dt;
    }

    // --- 追従カメラ（最小） ---
    // 追従対象の座標
    const auto& t = actor_->GetTransform();

    // 「後ろに5」「上に2」から見る
    DirectX::XMFLOAT3 eye{
        t.position_.x,
        t.position_.y + 2.0f,
        t.position_.z - 5.0f
    };

    // 少し上を注視
    DirectX::XMFLOAT3 at{
        t.position_.x,
        t.position_.y+1,
        t.position_.z
    };

    Camera::SetPosition({ eye.x, eye.y, eye.z });
    Camera::SetTarget({at.x, at.y, at.z});

    if(Input::IsKeyDown(VK_SPACE))
    {
        Sound::PlaySE("Assets/SE2.wav", 1.0f);
	}
	static bool isPlayBGM = false;
    if (Input::IsKeyDown('B'))
    {
        Sound::PlayBGM("Assets/BGM1.wav");
    }
}


void TestScene::Draw()
{
}

void TestScene::Release()
{
    actor_ = nullptr;
}
