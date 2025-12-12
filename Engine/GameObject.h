#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include "Transform.h"
#include "Component.h"

//-----------------------------------------------------------
// GameObject（Component対応・最小構成）
//
// - Transform を1つ持つ
// - 親子関係を持つ
// - Component を保持し、Update/Draw を伝播
//-----------------------------------------------------------
class GameObject
{
public:
    GameObject(GameObject* parent, const std::string& name);
    virtual ~GameObject();

    // ライフサイクル
    virtual void Initialize() {}
    virtual void Update() {}
    virtual void Draw() {}
    virtual void Release() {}

    // Component 管理
    void AddComponent(Component* component);

    // 親子管理
    void AddChild(GameObject* child);

    // ツリー更新
    void UpdateSub();
    void DrawSub();
    void ReleaseSub();

    // アクセサ
    Transform& GetTransform() { return transform_; }
    const Transform& GetTransform() const { return transform_; }

protected:
    std::string name_;

    GameObject* parent_;
    std::vector<GameObject*> children_;
    std::vector<Component*> components_;

    Transform transform_;
};
