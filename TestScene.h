#pragma once
#include "Engine/Scene.h"

class ModelComponent;

class TestScene :
    public Scene
{
public:
    TestScene();
    ~TestScene() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Release() override;

private:
	GameObject* actor_ = nullptr;
    GameObject* actor2_ = nullptr;
    GameObject* actor3_ = nullptr;
    //ModelComponent* mc_;
    //ModelComponent* mc2_;
    //ModelComponent* mc3_;
};

