#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include "DeviceResources.h"

namespace Gfx
{

    void Init(DX::DeviceResources* dr);
    void Reset();
    bool IsReady();


    ID3D11Device* Dev();
    ID3D11DeviceContext* Ctx();
    ID3D11RenderTargetView* RTV();
    ID3D11DepthStencilView* DSV();
    unsigned                 Width();
    unsigned                 Height();


    void WindowSizeChanged(unsigned w, unsigned h);


    DX::DeviceResources* Raw();
}
