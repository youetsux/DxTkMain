#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include "DeviceResources.h"

namespace Gfx
{
    // 非所有でグローバル登録（Appが寿命を保証）
    void Init(DX::DeviceResources* dr);
    void Reset();       // シャットダウン時にnullptrへ
    bool IsReady();

    // アクセサ
    ID3D11Device* Dev();
    ID3D11DeviceContext* Ctx();
    ID3D11RenderTargetView* RTV();
    ID3D11DepthStencilView* DSV();
    unsigned                 Width();
    unsigned                 Height();

    // サイズ変更
    void WindowSizeChanged(unsigned w, unsigned h);

    // 必要なら DeviceResources そのものを取得（非所有）
    DX::DeviceResources* Raw();
}
