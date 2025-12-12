#include "TestScene.h"
#include "TestScene.h"
#include "src/GameObject.h"
#include "src/Model.h"

//-----------------------------------------------------------
// モデル表示専用 GameObject
//-----------------------------------------------------------
class ModelTestObject : public GameObject
{
public:
    ModelTestObject(GameObject* parent)
        : GameObject(parent, "ModelTestObject")
        , modelHandle_(-1)
    {
    }

    ~ModelTestObject() override
    {
        if (modelHandle_ >= 0)
        {
            Model::Release(modelHandle_);
            modelHandle_ = -1;
        }
    }

    void Initialize() override
    {
        // ★表示したいモデルパスを指定
        modelHandle_ = Model::Load("Assets/Models/Test.fbx");

        // 自分の Transform を Model に紐づける
        Model::SetTransform(modelHandle_, transform_);

        // 表示位置を指定
        transform_.position_ = { 0.0f, 0.0f, 0.0f };
        transform_.scale_ = { 1.0f, 1.0f, 1.0f };
    }

    void Update() override
    {
    }

    void Draw() override
    {
        if (modelHandle_ >= 0)
        {
            Model::Draw(modelHandle_);
        }
    }

    void Release() override
    {
    }

private:
    int modelHandle_;
};

//-----------------------------------------------------------
// TestScene
//-----------------------------------------------------------
TestScene::TestScene()
{
}

TestScene::~TestScene()
{
    Release();
}

void TestScene::Initialize()
{
    // ルート配下にモデル表示用オブジェクトを1個生成
    // ルート配下にモデル表示用オブジェクトを1個生成
    auto* obj = new ModelTestObject(&Root());
    obj->Initialize();
}

void TestScene::Update()
{
}

void TestScene::Draw()
{
}

void TestScene::Release()
{
}
