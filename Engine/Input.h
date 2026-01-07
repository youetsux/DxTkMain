#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <Keyboard.h> // DirectXTK

//-----------------------------------------------------------

//




//

//   Input::Initialize(hwnd);

//   if (Input::IsKeyDown(VK_SPACE)) ...
//-----------------------------------------------------------
namespace Input
{
    void Initialize(HWND hwnd);
    void Update();


    void ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);


    bool IsKey(int vk);
    bool IsKeyDown(int vk);
    bool IsKeyUp(int vk);


    bool IsKey(DirectX::Keyboard::Keys key);
    bool IsKeyDown(DirectX::Keyboard::Keys key);
    bool IsKeyUp(DirectX::Keyboard::Keys key);
}
