#include "SceneManager.h"

#include "../TestScene.h"

//-----------------------------------------------------------
// コンストラクタ/デストラクタ
//-----------------------------------------------------------
SceneManager::SceneManager()
    : currentSceneID_(SCENE_ID_TEST)
    , nextSceneID_(SCENE_ID_TEST)
    , currentScene_(nullptr)
{
}

SceneManager::~SceneManager()
{
    Release();
}

//-----------------------------------------------------------
// 初期化
//-----------------------------------------------------------
void SceneManager::Initialize()
{
    currentSceneID_ = nextSceneID_;
    currentScene_ = CreateScene(currentSceneID_);
    if (currentScene_)
    {
        currentScene_->Initialize();
    }
}

//-----------------------------------------------------------
// 更新
//-----------------------------------------------------------
void SceneManager::Update()
{
    // シーン切り替え要求があれば反映
    if (nextSceneID_ != currentSceneID_)
    {
        DestroyScene(currentScene_);

        currentSceneID_ = nextSceneID_;
        currentScene_ = CreateScene(currentSceneID_);
        if (currentScene_)
        {
            currentScene_->Initialize();
        }
    }

    if (currentScene_)
    {
        // GameObject ツリーを回す（RootObject配下）
        currentScene_->Root().UpdateSub();

        // Scene 固有の更新（必要な場合のみ）
        currentScene_->Update();
    }
}

//-----------------------------------------------------------
// 描画
//-----------------------------------------------------------
void SceneManager::Draw()
{
    if (currentScene_)
    {
        // Scene 固有の描画（Terrain等）
        currentScene_->Draw();

        // GameObject ツリーの描画
        currentScene_->Root().DrawSub();
    }
}

//-----------------------------------------------------------
// 解放
//-----------------------------------------------------------
void SceneManager::Release()
{
    DestroyScene(currentScene_);
}

//-----------------------------------------------------------
// シーン切り替え（次フレーム反映）
//-----------------------------------------------------------
void SceneManager::ChangeScene(SCENE_ID next)
{
    nextSceneID_ = next;
}

//-----------------------------------------------------------
// Scene factory
//-----------------------------------------------------------
Scene* SceneManager::CreateScene(SCENE_ID id)
{
    switch (id)
    {
    case SCENE_ID_TEST:
        return new TestScene();
    default:
        return nullptr;
    }
}

void SceneManager::DestroyScene(Scene*& scene)
{
    if (!scene) return;

    scene->Release();
    delete scene;
    scene = nullptr;
}
