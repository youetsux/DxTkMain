#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <DirectXMath.h>
#include "Camera.h"

using namespace DirectX;

namespace {
    // 内部状態（ページの文面に合わせて、position_/target_/view/proj を保持）
    XMVECTOR gPosition = XMVectorSet(0.0f, 0.0f, -3.0f, 0.0f); // 視点
    XMVECTOR gTarget = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f); // 焦点

    XMMATRIX gView = XMMatrixIdentity();
    XMMATRIX gProj = XMMatrixIdentity();

    // 透視投影パラメータ（拡張）
    float gFovY = XM_PIDIV4;     // 45°
    float gAspect = 16.0f / 9.0f;
    float gZn = 0.1f;
    float gZf = 1000.0f;

    inline void RebuildView()
    {
        // 左手系LookAt（DirectXMathの標準）
        XMVECTOR up = XMVectorSet(0, 1, 0, 0);
        gView = XMMatrixLookAtLH(gPosition, gTarget, up);
    }

    inline void RebuildProj()
    {
        gProj = XMMatrixPerspectiveFovLH(gFovY, gAspect, gZn, gZf);
    }
} // anonymous

// --- インターフェース実装 ---

void Camera::Initialize()
{
    // 既定で投影・ビューを作っておく
    RebuildProj();
    RebuildView();
}

void Camera::Update()
{
    // 位置や焦点が外部から更新された前提でビルド
    RebuildView();
}

void Camera::SetPosition(XMVECTOR position)
{
    gPosition = position;
}

void Camera::SetTarget(XMVECTOR target)
{
    gTarget = target;
}

XMMATRIX Camera::GetViewMatrix()
{
    return gView;
}

XMMATRIX Camera::GetProjectionMatrix()
{
    return gProj;
}

// --- 拡張（任意）---

void Camera::SetPerspective(float fovYRadians, float aspect, float zn, float zf)
{
    gFovY = fovYRadians;
    gAspect = aspect;
    gZn = zn;
    gZf = zf;
    RebuildProj();
}

void Camera::OnResize(unsigned width, unsigned height)
{
    if (height == 0) return;
    gAspect = float(width) / float(height);
    RebuildProj();
}
