#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "App.h"

static App g_app;
static unsigned gW=1280, gH=720;
static bool g_ready=false;
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc);
    wc.style=CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    wc.lpfnWndProc=WndProc;
    wc.hInstance=hInst;
    wc.hCursor=LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName=L"DX11_DXTK_Renderer_Quad";
    RegisterClassExW(&wc);

    RECT rc{0,0,(LONG)gW,(LONG)gH}; AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"DX11 + DXTK Quad (POSITION-only)",
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                              rc.right-rc.left, rc.bottom-rc.top,
                              nullptr,nullptr,hInst,nullptr);

    g_app.Initialize(hwnd, gW, gH);
    g_ready = true;
    ShowWindow(hwnd, SW_SHOWDEFAULT);

    MSG msg{}; bool run=true;
    while (run)
    {
        while (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {
            if (msg.message == WM_QUIT) { run=false; break; }
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        if (!run) break;
        g_app.Update();
        g_app.Render();
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_SIZE:
        if (g_ready) { gW=LOWORD(l); gH=HIWORD(l); g_app.OnResize(gW,gH); }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0); return 0;
    case WM_KEYDOWN:
        if (w==VK_ESCAPE) DestroyWindow(hWnd); break;
    }
    return DefWindowProcW(hWnd,m,w,l);
}