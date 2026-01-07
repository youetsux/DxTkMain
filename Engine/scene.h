#pragma once
#include "RootObject.h"

//-----------------------------------------------------------



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
