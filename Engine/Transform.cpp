#include "Transform.h"

//----------------------------------------------
// コンストラクタ
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
// 非 const の既存 Calculation（設計維持）
// → ユーザーが直接行列操作したい用途を壊さない
//----------------------------------------------
void Transform::Calculation()
{
    matTranslate_ = XMMatrixTranslation(position_.x, position_.y, position_.z);

    XMMATRIX rx = XMMatrixRotationX(XMConvertToRadians(rotate_.x));
    XMMATRIX ry = XMMatrixRotationY(XMConvertToRadians(rotate_.y));
    XMMATRIX rz = XMMatrixRotationZ(XMConvertToRadians(rotate_.z));
    matRotate_ = rz * rx * ry;

    matScale_ = XMMatrixScaling(scale_.x, scale_.y, scale_.z);

    dirty_ = true;  // ★ 変更されたので dirty に
}

//----------------------------------------------
// const 用内部計算（既存設計を壊さない）
//----------------------------------------------
void Transform::CalculationInternal() const
{
    // 既存メンバ（matTranslate_ 等）も更新してよい
    // mutable にしている cachedWorld_ だけでなく、
    // 元の3行列も維持したまま更新する。

    XMMATRIX t = XMMatrixTranslation(position_.x, position_.y, position_.z);

    XMMATRIX rx = XMMatrixRotationX(XMConvertToRadians(rotate_.x));
    XMMATRIX ry = XMMatrixRotationY(XMConvertToRadians(rotate_.y));
    XMMATRIX rz = XMMatrixRotationZ(XMConvertToRadians(rotate_.z));

    XMMATRIX r = rz * rx * ry;
    XMMATRIX s = XMMatrixScaling(scale_.x, scale_.y, scale_.z);

    // ★ 既存メンバも更新
    const_cast<XMMATRIX&>(matTranslate_) = t;
    const_cast<XMMATRIX&>(matRotate_) = r;
    const_cast<XMMATRIX&>(matScale_) = s;

    // ワールド合成
    XMMATRIX world = s * r * t;

    if (pParent_)
    {
        world = world * pParent_->GetWorldMatrix();
    }

    cachedWorld_ = world;
    dirty_ = false;
}

//----------------------------------------------
// ワールド行列取得（const）
//----------------------------------------------
XMMATRIX Transform::GetWorldMatrix() const
{
    // 直接指定モードの時は常に CalculationInternal で OK
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
