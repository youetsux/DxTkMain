#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include "Transform.h"
#include "Component.h"

//-----------------------------------------------------------

//



//-----------------------------------------------------------
class GameObject
{
public:
    GameObject(GameObject* parent, const std::string& name);
    virtual ~GameObject();


    virtual void Initialize() {}
    virtual void Update() {}
    virtual void Draw() {}
    virtual void Release() {}


    void AddComponent(Component* component);


    void AddChild(GameObject* child);


    void UpdateSub();
    void DrawSub();
    void ReleaseSub();


    Transform& GetTransform() { return transform_; }
    const Transform& GetTransform() const { return transform_; }

protected:
    std::string name_;

    GameObject* parent_;
    std::vector<GameObject*> children_;
    std::vector<Component*> components_;

    Transform transform_;
};
