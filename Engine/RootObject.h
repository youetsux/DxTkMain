#pragma once
#include "GameObject.h"

//-----------------------------------------------------------
// RootObject : GameObjectツリーの根
//
// 変更点(2025-12-12):
// - SceneManager を Instantiate しない（SceneManagerは非GameObject化）
// - RootObject 自体は「空の根」として存在する（Transformは子へ伝播用）
//-----------------------------------------------------------
class RootObject : public GameObject
{
public:
    RootObject();
    ~RootObject();

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Release() override;
};
