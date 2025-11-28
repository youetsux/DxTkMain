// Minimal Win32 + DirectX11 template (DXTK-ready)
// - Single-file, C++17
// - Classic IDXGISwapChain (easy) + Resize handling
// - Placeholders for DirectXTK objects
//
// Build (Visual Studio):
//   - Multi-byte or Unicode OK
//   - Add library deps: d3d11.lib; dxgi.lib; d3dcompiler.lib
//   - If you use DirectXTK later: link Microsoft::DirectXTK (vcpkg) or add DXTK sources
//
// Notes:
//   - Game-style loop uses PeekMessage (non-blocking)
//   - WM_SIZE → Recreate RTV
//   - NOMINMAX to avoid std::min/max conflict

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cassert>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

// -----------------------------------------------------------------------------
// Globals (keep minimal)
// -----------------------------------------------------------------------------
HINSTANCE                   g_hInst = nullptr;
HWND                        g_hWnd = nullptr;
UINT                        g_width = 1280;
UINT                        g_height = 720;

ComPtr<ID3D11Device>        g_device;
ComPtr<ID3D11DeviceContext> g_context;
ComPtr<IDXGISwapChain>      g_swapchain;
ComPtr<ID3D11RenderTargetView> g_rtv;

// (Optional) DirectXTK objects — uncomment when you add DXTK
 //#include <SpriteBatch.h>
 //#include <SpriteFont.h>
 //#include <CommonStates.h>
 //std::unique_ptr<DirectX::SpriteBatch>  g_spriteBatch;
 //std::unique_ptr<DirectX::SpriteFont>   g_spriteFont;
 //std::unique_ptr<DirectX::CommonStates> g_states;

// -----------------------------------------------------------------------------
// Forward decls
// -----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void CreateDeviceResources();
void CreateWindowSizeDependentResources();
void OnRender();
void OnResize(UINT w, UINT h);

// -----------------------------------------------------------------------------
// Entry
// -----------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    g_hInst = hInstance;

    // Register class
    WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"DXTK_MinWin";
    RegisterClassExW(&wc);

    // Create window
    RECT rc{ 0,0,(LONG)g_width,(LONG)g_height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindowW(wc.lpszClassName, L"DX11 Minimal (DXTK-ready)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr);
    ShowWindow(g_hWnd, SW_SHOWDEFAULT);

    // D3D init
    CreateDeviceResources();
    CreateWindowSizeDependentResources();

    // (Optional) Init DXTK after device/context are ready
    // g_states = std::make_unique<DirectX::CommonStates>(g_device.Get());
    // g_spriteBatch = std::make_unique<DirectX::SpriteBatch>(g_context.Get());
    // g_spriteFont  = std::make_unique<DirectX::SpriteFont>(g_device.Get(), L"assets/ui/UIFont.spritefont");

    // Main loop
    MSG msg{};
    bool running = true;
    while (running)
    {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) { running = false; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running) break;

        // TODO: Update();
        OnRender();
    }

    return (int)msg.wParam;
}

// -----------------------------------------------------------------------------
// D3D Setup
// -----------------------------------------------------------------------------
void CreateDeviceResources()
{
    UINT flags = 0;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL created = {};

    ComPtr<IDXGIFactory> factory;
    HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)factory.GetAddressOf());
    assert(SUCCEEDED(hr));

    // Create device
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        flags, levels, _countof(levels), D3D11_SDK_VERSION,
        g_device.GetAddressOf(), &created, g_context.GetAddressOf());
    assert(SUCCEEDED(hr));

    // Create swapchain (classic, easy path)
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = g_width;
    sd.BufferDesc.Height = g_height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; // simple

    hr = factory->CreateSwapChain(g_device.Get(), &sd, g_swapchain.GetAddressOf());
    assert(SUCCEEDED(hr));

    // Disable Alt+Enter default toggle
    factory->MakeWindowAssociation(g_hWnd, DXGI_MWA_NO_ALT_ENTER);
}

void CreateWindowSizeDependentResources()
{
    // Release old RTV
    g_rtv.Reset();

    // Resize swapchain buffers if already created
    if (g_swapchain)
    {
        g_swapchain->ResizeBuffers(2, g_width, g_height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

        ComPtr<ID3D11Texture2D> backbuf;
        HRESULT hr = g_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backbuf.GetAddressOf());
        assert(SUCCEEDED(hr));
        hr = g_device->CreateRenderTargetView(backbuf.Get(), nullptr, g_rtv.GetAddressOf());
        assert(SUCCEEDED(hr));

        // Bind default RTV
        g_context->OMSetRenderTargets(1, g_rtv.GetAddressOf(), nullptr);

        // Setup viewport
        D3D11_VIEWPORT vp{}; vp.TopLeftX = 0; vp.TopLeftY = 0; vp.Width = (FLOAT)g_width; vp.Height = (FLOAT)g_height; vp.MinDepth = 0; vp.MaxDepth = 1;
        g_context->RSSetViewports(1, &vp);
    }
}

void OnRender()
{
    if (!g_context || !g_rtv) return;

    const float clear[4]{ 0.10f, 0.18f, 0.25f, 1.0f };
    g_context->ClearRenderTargetView(g_rtv.Get(), clear);

    // (Optional) DXTK 2D drawing
     //g_spriteBatch->Begin();
     //g_spriteFont->DrawString(g_spriteBatch.get(), L"Hello DXTK", {20,20});
     //g_spriteBatch->End();

    g_swapchain->Present(1, 0);
}

void OnResize(UINT w, UINT h)
{
    if (w == 0 || h == 0) return; // minimized
    g_width = w; g_height = h;
    CreateWindowSizeDependentResources();
}

// -----------------------------------------------------------------------------
// WndProc
// -----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE:
        OnResize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) { DestroyWindow(hWnd); }
        break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
