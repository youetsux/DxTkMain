#pragma once

class GameObject;

//-----------------------------------------------------------

//



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
