#include "Transform.h"

//----------------------------------------------

//----------------------------------------------
Transform::Transform()
    : pParent_(nullptr)
    , isSetDirect(false)
    , dirty_(true)
{
    position_ = XMFLOAT3(0, 0, 0);
    rotate_ = XMFLOAT3(0, 0, 0);
    scale_ = XMFLOAT3(1, 1, 1);

    matTranslate_ = XMMatrixIdentity();
    matRotate_ = XMMatrixIdentity();
    matScale_ = XMMatrixIdentity();

    cachedWorld_ = XMMatrixIdentity();
}

Transform::~Transform()
{
}

//----------------------------------------------


//----------------------------------------------
void Transform::Calculation()
{
    matTranslate_ = XMMatrixTranslation(position_.x, position_.y, position_.z);

    XMMATRIX rx = XMMatrixRotationX(XMConvertToRadians(rotate_.x));
    XMMATRIX ry = XMMatrixRotationY(XMConvertToRadians(rotate_.y));
    XMMATRIX rz = XMMatrixRotationZ(XMConvertToRadians(rotate_.z));
    matRotate_ = rz * rx * ry;

    matScale_ = XMMatrixScaling(scale_.x, scale_.y, scale_.z);

    dirty_ = true;
}

//----------------------------------------------

//----------------------------------------------
void Transform::CalculationInternal() const
{




    XMMATRIX t = XMMatrixTranslation(position_.x, position_.y, position_.z);

    XMMATRIX rx = XMMatrixRotationX(XMConvertToRadians(rotate_.x));
    XMMATRIX ry = XMMatrixRotationY(XMConvertToRadians(rotate_.y));
    XMMATRIX rz = XMMatrixRotationZ(XMConvertToRadians(rotate_.z));

    XMMATRIX r = rz * rx * ry;
    XMMATRIX s = XMMatrixScaling(scale_.x, scale_.y, scale_.z);


    const_cast<XMMATRIX&>(matTranslate_) = t;
    const_cast<XMMATRIX&>(matRotate_) = r;
    const_cast<XMMATRIX&>(matScale_) = s;


    XMMATRIX world = s * r * t;

    if (pParent_)
    {
        world = world * pParent_->GetWorldMatrix();
    }

    cachedWorld_ = world;
    dirty_ = false;
}

//----------------------------------------------

//----------------------------------------------
XMMATRIX Transform::GetWorldMatrix() const
{

    if (!isSetDirect)
    {
        if (dirty_)
        {
            CalculationInternal();
        }
    }
    else
    {
        if (dirty_)
        {
            CalculationInternal();
        }
    }

    return cachedWorld_;
}
