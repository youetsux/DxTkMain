#pragma once

#pragma once
#include <cmath>
#include <cfloat>
#include <DirectXMath.h>

struct BVolume
{
    DirectX::XMFLOAT3 min;
    DirectX::XMFLOAT3 max;
    DirectX::XMFLOAT3 center;
    float              radius;

    BVolume()
    {
        Reset();
    }

    void Reset()
    {
        min = DirectX::XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
        max = DirectX::XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        center = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
        radius = 1.0f;
    }


    void WrapBox(const DirectX::XMFLOAT3& p)
    {
        if (p.x < min.x) min.x = p.x;
        if (p.y < min.y) min.y = p.y;
        if (p.z < min.z) min.z = p.z;

        if (p.x > max.x) max.x = p.x;
        if (p.y > max.y) max.y = p.y;
        if (p.z > max.z) max.z = p.z;
    }


    void RecalcSphereFromAABB()
    {
        using namespace DirectX;

        center.x = (min.x + max.x) * 0.5f;
        center.y = (min.y + max.y) * 0.5f;
        center.z = (min.z + max.z) * 0.5f;

        float dx = max.x - center.x;
        float dy = max.y - center.y;
        float dz = max.z - center.z;

        float r2 = dx * dx + dy * dy + dz * dz;
        if (r2 <= 0.0f) {
            radius = 1.0f;
        }
        else {
            radius = std::sqrt(r2);
        }
    }

    void Scale(float s)
    {
        min.x *= s;  min.y *= s;  min.z *= s;
        max.x *= s;  max.y *= s;  max.z *= s;

        center.x *= s;
        center.y *= s;
        center.z *= s;

        radius *= s;
    }
};