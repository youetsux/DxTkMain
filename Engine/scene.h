#pragma once
#include "RootObject.h"

//-----------------------------------------------------------
// Scene : Transform を持たない「世界の管理者」
// - GameObject ツリーは RootObject 配下にぶら下げる
// - Scene 自体は GameObject ではない（Transform不要）
//-----------------------------------------------------------
class Scene
{
public:
    Scene() = default;
    virtual ~Scene() = default;

    virtual void Initialize() = 0;
    virtual void Update() = 0;
    virtual void Draw() = 0;
    virtual void Release() = 0;

    RootObject& Root() { return root_; }
    const RootObject& Root() const { return root_; }

protected:
    RootObject root_;
};
