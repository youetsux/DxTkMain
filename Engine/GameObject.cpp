#include "GameObject.h"

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
// Component
//-----------------------------------------------------------
void GameObject::AddComponent(Component* component)
{
    if (!component) return;

    components_.push_back(component);
    component->Initialize();
}

//-----------------------------------------------------------
// 親子
//-----------------------------------------------------------
void GameObject::AddChild(GameObject* child)
{
    if (!child) return;

    children_.push_back(child);
    child->parent_ = this;
    child->transform_.pParent_ = &transform_;
}

//-----------------------------------------------------------
// ツリー更新
//-----------------------------------------------------------
void GameObject::UpdateSub()
{
    Update();
    // ★ここで Transform を計算
    transform_.Calculation();

    for (Component* c : components_)
    {
        c->Update();
    }

    for (GameObject* child : children_)
    {
        child->UpdateSub();
    }
}

void GameObject::DrawSub()
{
    Draw();

    for (Component* c : components_)
    {
        c->Draw();
    }

    for (GameObject* child : children_)
    {
        child->DrawSub();
    }
}

void GameObject::ReleaseSub()
{
    Release();

    for (Component* c : components_)
    {
        c->Release();
        delete c;
    }
    components_.clear();

    for (GameObject* child : children_)
    {
        child->ReleaseSub();
        delete child;
    }
    children_.clear();
}
