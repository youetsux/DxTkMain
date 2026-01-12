//#include "TestScene.h"
//
//#include "Engine/GameObject.h"
//#include "ModelComponent.h"
//#include "Engine/Input.h"
//#include "Engine/Camera.h"
//#include "Engine/EngineTime.h"
//#include "Engine/sound.h"
//#include "Engine/Gfx.h"
//#include <DirectXMath.h>
//#include "Engine/BakedPoseEvalTest.h"
//
//#ifndef BAKED_POSE_EVAL_SELFTESTS
//#define BAKED_POSE_EVAL_SELFTESTS 0
//#endif
//
//
//TestScene::TestScene()
//{
//}
//
//TestScene::~TestScene()
//{
//	Release();
//}
//
//void TestScene::Initialize()
//{
//	Camera::SetPosition({ 0.0f, 1.0f, -3.0f });
//	Camera::SetTarget({ 0.0, 1.0, 0.0 });
//
//	actor_ = new GameObject(&Root(), "Actor");
//	auto mc = new ModelComponent(actor_, "Assets/BoyJump.fbx", true);
//
//	// モデル（必要に応じてパス・正規化高さを調整）
//	actor_->AddComponent(mc);
//
//	// 初期位置 mc_
//	actor_->GetTransform().position_ = { 0.0f, 0.5f, 0.0f };
//	actor_->GetTransform().scale_ = { 1.0f, 1.0f, 1.0f };
//	mc->SetAnimStack(0);
//	mc->SetAnimRange(0, 92, 1.0f);
//	mc->SetLoop(true);
//
//	//2体目
//	actor2_ = new GameObject(&Root(), "Actor2");
//	auto mc2 = new ModelComponent(actor2_, "Assets/TriAvater.fbx", true);
//
//	// モデル（必要に応じてパス・正規化高さを調整）
//	actor2_->AddComponent(mc2);
//	mc2->SetAnimStack(1);
//	mc2->SetAnimRange(0, 229, 1.0f);
//	mc2->SetLoop(true);
//	//mc2->SetRootScale(0.0001f);
//	//mc2->SetRootRotationYawPitchRoll(0, DirectX::XMConvertToRadians( - 90.0f), 0);
//	//mc2->SetRootRotationYawPitchRollDeg(0, 90, 0);
//	// 初期位置 mc_
//	actor2_->GetTransform().position_ = { 1.0f, 0.0f, 0.0f };
//	actor2_->GetTransform().rotate_ = { 90.0f, 0.0f, 0.0f };//blenderのモデルだけ90度寝てる
//	//actor2_->GetTransform().scale_ = { 0.0001f, 0.0001f, 0.0001f };
//
//
//	//3体目
//	actor3_ = new GameObject(&Root(), "Actor3");
//	auto mc3 = new ModelComponent(actor3_, "Assets/SillyDancing.fbx", true);
//
//	// モデル（必要に応じてパス・正規化高さを調整）
//	actor3_->AddComponent(mc3);
//	mc3->SetAnimStack(1);
//	mc3->SetAnimRange(0, 229, 1.0f);
//	mc3->SetLoop(true);
//	// 初期位置 mc_
//	actor3_->GetTransform().position_ = { -1.0f, 0.0f, 0.0f };
//	actor3_->GetTransform().scale_ = { 0.01f, 0.01f, 0.01f };
//
//
//}
//
//void TestScene::Update()
//{
//	if (!actor_) return;
//
//	const float dt = (float)EngineTime::DeltaTime();
//
//	// --- 入力で左右に動かす（追従が分かりやすいので一緒に） ---
//	if (Input::IsKey(VK_LEFT))
//	{
//		actor_->GetTransform().position_.x -= 1.0f * dt;
//	}
//	if (Input::IsKey(VK_RIGHT))
//	{
//		actor_->GetTransform().position_.x += 1.0f * dt;
//	}
//
//
//	// --- 追従カメラ（最小） ---
//	// 追従対象の座標
//	const auto& t = actor_->GetTransform();
//
//	// 「後ろに5」「上に2」から見る
//	DirectX::XMFLOAT3 eye{
//		t.position_.x,
//		t.position_.y + 2.0f,
//		t.position_.z - 5.0f
//	};
//
//	// 少し上を注視
//	DirectX::XMFLOAT3 at{
//		t.position_.x,
//		t.position_.y + 1,
//		t.position_.z
//	};
//
//	//Camera::SetPosition({ eye.x, eye.y, eye.z });
//	//Camera::SetTarget({at.x, at.y, at.z});
//
//	if (Input::IsKeyDown(VK_SPACE))
//	{
//		//int acount = mc_->GetAnimStackCount();
//		//static int animNum = 0;
//		//mc_->SetAnimStack((animNum++) % acount);
//
//	}
//	static bool isPlayBGM = false;
//	if (Input::IsKeyDown('B'))
//	{
//		Sound::PlayBGM("Assets/BGM1.wav");
//	}
//
//	// Debug: run baked pose evaluator self tests.
//	// Press 'T' to run.
//#if BAKED_POSE_EVAL_SELFTESTS
//	if (Input::IsKeyDown('T'))
//	{
//		BakedPoseEvalTest::SelfTest_WorldOrderIndependent();
//		BakedPoseEvalTest::SelfTest_PartialChannelTRS();
//	}
//#endif
//
//}
//
//
//void TestScene::Draw()
//{
//}
//
//void TestScene::Release()
//{
//	actor_ = nullptr;
//}

#include "TestScene.h"

#include "Engine/GameObject.h"
#include "ModelComponent.h"
#include "Engine/Input.h"
#include "Engine/Camera.h"
#include "Engine/EngineTime.h"
#include "Engine/sound.h"
#include "Engine/Gfx.h"
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
    Camera::SetPosition({ 0.0f, 1.0f, -300.0f });
    Camera::SetTarget({ 0.0f, 1.0f, 0.0f });

    // 1体目
    actor_ = new GameObject(&Root(), "Actor");
    auto mc = new ModelComponent(actor_, "Assets/BoyJump.fbx");
    actor_->AddComponent(mc);

    //actor_->GetTransform().position_ = { 0.0f, 0.7f, 0.0f };
    //actor_->GetTransform().rotate_ = { 90.0f, 0.0f, 0.0f };
    //actor_->GetTransform().scale_ = { 0.8f, 0.8f, 0.8f };

    mc->SetAnimStack(0);
    mc->SetAnimRange(0, 92, 1.0f);
    mc->SetLoop(true);

    // 2体目
    actor2_ = new GameObject(&Root(), "Actor2");
    auto mc2 = new ModelComponent(actor2_, "Assets/TriAvater.fbx", 1.0f);
    actor2_->AddComponent(mc2);

    // 以前のコードに合わせて「見た目」を固定（必要ならここは調整）
    actor2_->GetTransform().position_ = { 1.0f, 0.0f, 0.0f };
    actor2_->GetTransform().rotate_ = { 90.0f, 0.0f, 0.0f };
    actor2_->GetTransform().scale_ = { 1.0f, 1.0f, 1.0f };

    //mc2->SetAnimStack(0);
    //mc2->SetAnimRange(0, 0, 1.0f);
    //mc2->SetLoop(false);

    // 3体目
    actor3_ = new GameObject(&Root(), "Actor3");
    auto mc3 = new ModelComponent(actor3_, "Assets/SillyDancing.fbx", 1.0f);
    actor3_->AddComponent(mc3);

    actor3_->GetTransform().position_ = { -1.0f, 0.0f, 0.0f };
    actor3_->GetTransform().rotate_ = { 0.0f, 0.0f, 0.0f };
    actor3_->GetTransform().scale_ = { 0.01f, 0.01f, 0.01f };

    mc3->SetAnimStack(1);
    mc3->SetAnimRange(0, 229, 1.0f);
    mc3->SetLoop(true);
}

void TestScene::Update()
{
    if (!actor_) return;

    const float dt = (float)EngineTime::DeltaTime();

    if (Input::IsKey(VK_LEFT))
    {
        actor_->GetTransform().position_.x -= 1.0f * dt;
    }
    if (Input::IsKey(VK_RIGHT))
    {
        actor_->GetTransform().position_.x += 1.0f * dt;
    }

    if (Input::IsKeyDown('B'))
    {
        Sound::PlayBGM("Assets/BGM1.wav");
    }

    // カメラ追従（必要なら有効化）
    /*
    const auto& t = actor_->GetTransform();

    DirectX::XMFLOAT3 eye{
        t.position_.x,
        t.position_.y + 2.0f,
        t.position_.z - 5.0f
    };

    DirectX::XMFLOAT3 at{
        t.position_.x,
        t.position_.y + 1.0f,
        t.position_.z
    };

    Camera::SetPosition({ eye.x, eye.y, eye.z });
    Camera::SetTarget({ at.x, at.y, at.z });
    */
}

void TestScene::Draw()
{
}

void TestScene::Release()
{
    actor_ = nullptr;
    actor2_ = nullptr;
    actor3_ = nullptr;
}

