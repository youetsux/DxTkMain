DX11 + DirectXTK Granular Renderer (Begin/End/Present split)
============================================================

HOW TO BUILD (Visual Studio):
1) Create a new empty C++ Windows desktop project (x64 or Win32).
2) Add the 'src' folder files to the project.
3) Install NuGet package:  directxtk_desktop_2019
   (This provides SpriteBatch/SpriteFont/CommonStates headers/libs.)
4) Build & run. Press ESC to quit.

Structure:
- DeviceResources.h/.cpp : D3D11 device/context/swapchain/RTV/DSV encapsulation
- Renderer.h/.cpp        : Thin renderer (BeginFrame/EndFrame/Present) + DirectXTK objects
- App.h/.cpp             : App loop hooks (Update/Render)
- Main.cpp               : Window creation + message loop (Initialize BEFORE ShowWindow)

Notes:
- This sample uses DXGI_SWAP_EFFECT_DISCARD for clarity.
- If you want Flip Model, change swapchain creation accordingly.
- To draw fonts, uncomment SpriteFont initialization in Renderer and place a .spritefont file at assets/ui/UIFont.spritefont
