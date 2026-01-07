#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <DirectXMath.h>
#include "Camera.h"

using namespace DirectX;

namespace {

    XMVECTOR gPosition = XMVectorSet(0.0f, 0.0f, -3.0f, 0.0f);
    XMVECTOR gTarget = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

    XMMATRIX gView = XMMatrixIdentity();
    XMMATRIX gProj = XMMatrixIdentity();


    float gFovY = XM_PIDIV4;
    float gAspect = 16.0f / 9.0f;
    float gZn = 0.1f;
    float gZf = 1000.0f;

    inline void RebuildView()
    {

        XMVECTOR up = XMVectorSet(0, 1, 0, 0);
        gView = XMMatrixLookAtLH(gPosition, gTarget, up);
    }

    inline void RebuildProj()
    {
        gProj = XMMatrixPerspectiveFovLH(gFovY, gAspect, gZn, gZf);
    }
} // anonymous



void Camera::Initialize()
{

    RebuildProj();
    RebuildView();
}

void Camera::Update()
{

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
