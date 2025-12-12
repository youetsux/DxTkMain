#pragma once
#include <vector>
#include <string>
#include "Transform.h"

//-----------------------------------------------------------
// GameObject（最小構成）
//
// 責務:
// - Transform を1つ持つ
// - 親子関係を持つ
// - Update / Draw を子へ伝播
//
// 削除したもの:
// - Collider
// - Direct3D / 描画依存
// - Scene / Manager 依存
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

    // 親子管理
    void AddChild(GameObject* child);
    void RemoveChild(GameObject* child);

    // ツリー更新
    void UpdateSub();
    void DrawSub();
    void ReleaseSub();

    // アクセサ
    Transform& GetTransform() { return transform_; }
    const Transform& GetTransform() const { return transform_; }

    GameObject* GetParent() const { return parent_; }
    const std::vector<GameObject*>& GetChildren() const { return children_; }

protected:
    std::string name_;

    GameObject* parent_;
    std::vector<GameObject*> children_;

    Transform transform_;
};
