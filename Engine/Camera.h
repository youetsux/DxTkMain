#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <DirectXMath.h>


// Initialize / Update / SetPosition / SetTarget / GetViewMatrix / GetProjectionMatrix

namespace Camera
{
    using namespace DirectX;


    void Initialize();


    void Update();


    void SetPosition(XMVECTOR position);


    void SetTarget(XMVECTOR target);


    XMMATRIX GetViewMatrix();


    XMMATRIX GetProjectionMatrix();



    void SetPerspective(float fovYRadians, float aspect, float zn = 0.1f, float zf = 1000.0f);


    void OnResize(unsigned width, unsigned height);
}
