#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <Keyboard.h> // DirectXTK

//-----------------------------------------------------------
// Input（DXTK版）
//
// 方針:
// - ゲーム層の基本APIは VK_*（int）互換
// - DXTK Keys は補助としてオーバーロード提供
// - DXTK 依存は Input 内に閉じ込める
//
// 使い方:
//   Input::Initialize(hwnd);
//   毎フレーム Input::Update();
//   if (Input::IsKeyDown(VK_SPACE)) ...
//-----------------------------------------------------------
namespace Input
{
    void Initialize(HWND hwnd);
    void Update();

    // ★重要：WndProc から呼ぶ
    void ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);

    // --- 正式API（VK_*）---
    bool IsKey(int vk);
    bool IsKeyDown(int vk);
    bool IsKeyUp(int vk);

    // --- 補助API（DXTK Keys）---
    bool IsKey(DirectX::Keyboard::Keys key);
    bool IsKeyDown(DirectX::Keyboard::Keys key);
    bool IsKeyUp(DirectX::Keyboard::Keys key);
}
