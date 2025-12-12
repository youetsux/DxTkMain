#include "GameObject.h"
#include <algorithm>

//-----------------------------------------------------------
// コンストラクタ / デストラクタ
//-----------------------------------------------------------
GameObject::GameObject(GameObject* parent, const std::string& name)
    : name_(name)
    , parent_(nullptr)
{
    if (parent)
    {
        parent->AddChild(this);
    }
}

GameObject::~GameObject()
{
    ReleaseSub();
}

//-----------------------------------------------------------
// 親子管理
//-----------------------------------------------------------
void GameObject::AddChild(GameObject* child)
{
    if (!child) return;

    children_.push_back(child);
    child->parent_ = this;

    // Transform の親子関係も接続
    child->transform_.pParent_ = &transform_;
}

void GameObject::RemoveChild(GameObject* child)
{
    if (!child) return;

    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end())
    {
        (*it)->parent_ = nullptr;
        (*it)->transform_.pParent_ = nullptr;
        children_.erase(it);
    }
}

//-----------------------------------------------------------
// ツリー更新
//-----------------------------------------------------------
void GameObject::UpdateSub()
{
    Update();

    for (GameObject* child : children_)
    {
        if (child)
        {
            child->UpdateSub();
        }
    }
}

void GameObject::DrawSub()
{
    Draw();

    for (GameObject* child : children_)
    {
        if (child)
        {
            child->DrawSub();
        }
    }
}

void GameObject::ReleaseSub()
{
    Release();

    for (GameObject* child : children_)
    {
        if (child)
        {
            child->ReleaseSub();
            delete child;
        }
    }
    children_.clear();
}
