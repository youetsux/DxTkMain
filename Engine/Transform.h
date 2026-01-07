#pragma once
#include <DirectXMath.h>

using namespace DirectX;

class Transform
{
public:

    XMMATRIX matTranslate_;
    XMMATRIX matRotate_;
    XMMATRIX matScale_;
    XMFLOAT3 position_;
    XMFLOAT3 rotate_;
    XMFLOAT3 scale_;
    Transform* pParent_;
    bool isSetDirect;

private:

    mutable bool dirty_;
    mutable XMMATRIX cachedWorld_;

public:
    Transform();
    ~Transform();

    void Calculation();
    void CalculationInternal() const;
    XMMATRIX GetWorldMatrix() const;

    static XMFLOAT3 Float3Add(XMFLOAT3 a, XMFLOAT3 b)
    {
        return XMFLOAT3(a.x + b.x, a.y + b.y, a.z + b.z);
    }
};
