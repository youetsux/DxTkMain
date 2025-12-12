#pragma once
#include "src/Scene.h"


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
};

