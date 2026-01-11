#pragma once

class GameObject;

//-----------------------------------------------------------
// Component（最小基底）
//
// 責務:
// - 所有者 GameObject を知っている
// - Update / Draw / Release の受け口
//-----------------------------------------------------------
class Component
{
public:
    explicit Component(GameObject* owner)
        : owner_(owner)
    {
    }

    virtual ~Component() = default;

    virtual void Initialize() {}
    virtual void Update() {}
    virtual void Draw() {}
    virtual void Release() {}

    GameObject* GetOwner() const { return owner_; }

protected:
    GameObject* owner_;
};
