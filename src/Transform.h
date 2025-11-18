#pragma once
#include <DirectXMath.h>

using namespace DirectX;

class Transform
{
public:
    // 既存メンバはすべてそのまま維持
    XMMATRIX matTranslate_;
    XMMATRIX matRotate_;
    XMMATRIX matScale_;
    XMFLOAT3 position_;
    XMFLOAT3 rotate_;
    XMFLOAT3 scale_;
    Transform* pParent_;
    bool isSetDirect;

private:
    // ★ 追加：内部キャッシュと dirty フラグ
    mutable bool dirty_;
    mutable XMMATRIX cachedWorld_;

public:
    Transform();
    ~Transform();

    void Calclation();                 // 設計は維持（非 const のまま）
    void CalclationInternal() const;   // ★ const 版内部計算を追加
    XMMATRIX GetWorldMatrix() const;   // ★ const 対応

    static XMFLOAT3 Float3Add(XMFLOAT3 a, XMFLOAT3 b)
    {
        return XMFLOAT3(a.x + b.x, a.y + b.y, a.z + b.z);
    }
};
