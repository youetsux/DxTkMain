#include "SceneManager.h"

#include "../TestScene.h"


#include "Renderer.h"

//-----------------------------------------------------------

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

//-----------------------------------------------------------
void SceneManager::Update()
{

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

        currentScene_->Root().UpdateSub();


        currentScene_->Update();
    }
}

//-----------------------------------------------------------

//-----------------------------------------------------------
void SceneManager::Draw()
{
    if (currentScene_)
    {

        currentScene_->Draw();


        currentScene_->Root().DrawSub();
    }
}

//-----------------------------------------------------------



//-----------------------------------------------------------
void SceneManager::SubmitDraw(Renderer& /*renderer*/)
{
    if (currentScene_)
    {

        currentScene_->Draw();
        currentScene_->Root().DrawSub();
    }
}

//-----------------------------------------------------------

//-----------------------------------------------------------
void SceneManager::Release()
{
    DestroyScene(currentScene_);
}

//-----------------------------------------------------------

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
