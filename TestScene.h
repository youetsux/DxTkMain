#pragma once
#include "Engine/Scene.h"


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
};

