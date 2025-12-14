#include "SceneManager.h"

#include "../TestScene.h"

// ★変更：SubmitDraw の引数型のため（将来的にRendererへ要求を積む）
#include "Renderer.h"

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
// 描画（既存：即時描画経路）
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
// ★変更：段階移行用 SubmitDraw
//  - この段階では Draw() と同じ処理（動作維持）
//  - 次のステップで Component 側を Submit 方式に寄せていく
//-----------------------------------------------------------
void SceneManager::SubmitDraw(Renderer& /*renderer*/)
{
    if (currentScene_)
    {
        // 現段階では “積む” ではなく、従来通り描画を実行して動作維持
        currentScene_->Draw();
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
