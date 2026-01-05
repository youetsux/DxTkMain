// Main.cpp  (方針A：可変dtに統一 / Updateは1回/フレーム)
// - EngineTime::Tick(elapsed) は 1回/フレーム
// - g_app.Update() も 1回/フレーム
// - Input::ProcessMessage は WndProc で維持
// - Render は（任意で）60Hz目標で SleepUntil によるペーシングは残す（dt自体は可変のまま）

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>   // timeBeginPeriod / timeEndPeriod
#include <algorithm>   // std::max
#include <cmath>
#include "App.h"
#include "..\Engine\EngineTime.h"
#include "..\Engine\Input.h"

#pragma comment(lib, "winmm.lib")

static App g_app;
static unsigned gW = 1280, gH = 720;
static bool g_ready = false;
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// 高精度で残り時間だけ待つ（Sleep + busy-wait ハイブリッド）
static void SleepUntil(double targetTime, double spinThreshold, const LARGE_INTEGER& freq)
{
    LARGE_INTEGER cur;
    QueryPerformanceCounter(&cur);
    double now = double(cur.QuadPart) / double(freq.QuadPart);
    double remaining = targetTime - now;
    if (remaining <= 0.0) return;

    if (remaining > spinThreshold) {
        DWORD sleepMs = (DWORD)std::max(0.0, (remaining - spinThreshold) * 1000.0);
        if (sleepMs > 0) Sleep(sleepMs);
    }

    // busy-wait
    do {
        QueryPerformanceCounter(&cur);
        now = double(cur.QuadPart) / double(freq.QuadPart);
    } while (now < targetTime);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    (void)hr;

    WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"DX11_App";
    RegisterClassExW(&wc);

    RECT rc{ 0,0,(LONG)gW,(LONG)gH }; AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"MyApp",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInst, nullptr);

    g_app.Initialize(hwnd, gW, gH);
    g_ready = true;
    ShowWindow(hwnd, SW_SHOWDEFAULT);

    timeBeginPeriod(1); // Sleep 精度向上（終了時に timeEndPeriod(1)）

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);

    // Render の目標間隔（ペーシング用）。dt は可変のまま（elapsed を Tick する）
    const double targetDt = 1.0 / 60.0;
    const double spinThreshold = 0.003;

    LARGE_INTEGER prevCnt; QueryPerformanceCounter(&prevCnt);
    double prevTime = double(prevCnt.QuadPart) / double(freq.QuadPart);

    double lastRenderTime = prevTime;

    // FPS 表示変数
    double fpsTimeAccum = 0.0;
    int    fpsFrames = 0;
    const double fpsUpdateInterval = 1.0;   // 1秒ごとの平均FPS
    wchar_t titleBuf[256];
    const wchar_t* baseTitle = L"MyApp";

    MSG msg{};
    bool run = true;
    while (run)
    {
        // メッセージ処理
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { run = false; break; }
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        if (!run) break;

        // 時刻更新（実測フレーム時間）
        LARGE_INTEGER nowCnt; QueryPerformanceCounter(&nowCnt);
        double now = double(nowCnt.QuadPart) / double(freq.QuadPart);
        double elapsed = now - prevTime;      // ★実測dt（可変）
        prevTime = now;

        // ★ EngineTime は可変dtで更新（1フレームに1回だけ）
        EngineTime::Tick(elapsed);

        // ★ Update は 1フレームに1回だけ（可変dt前提）
        g_app.Update();

        // 次の描画時刻を計算（ペーシングは維持：dt自体は Tick 済みなので変えない）
        double nextRenderTime = lastRenderTime + targetDt;
        if (now < nextRenderTime) {
            SleepUntil(nextRenderTime, spinThreshold, freq);
            QueryPerformanceCounter(&nowCnt);
            now = double(nowCnt.QuadPart) / double(freq.QuadPart);
        }

        // 描画
        g_app.Render();

        // 実描画時刻を記録
        LARGE_INTEGER afterCnt; QueryPerformanceCounter(&afterCnt);
        lastRenderTime = double(afterCnt.QuadPart) / double(freq.QuadPart);

        // FPS計測：実測elapsedで1秒ごとの平均FPSを出す
        fpsFrames++;
        fpsTimeAccum += elapsed;

        if (fpsTimeAccum >= fpsUpdateInterval) {
            double fps = double(fpsFrames) / fpsTimeAccum;
            swprintf_s(titleBuf, _countof(titleBuf), L"%s - FPS: %.1f", baseTitle, fps);
            SetWindowTextW(hwnd, titleBuf);

            fpsTimeAccum = 0.0;
            fpsFrames = 0;
        }
    }

    timeEndPeriod(1);
    CoUninitialize();
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT m, WPARAM w, LPARAM l)
{
    if (g_ready)
    {
        Input::ProcessMessage(m, w, l); // ★維持：DXTK Keyboard にメッセージを渡す
    }

    switch (m) {
    case WM_SIZE:
        if (g_ready) { gW = LOWORD(l); gH = HIWORD(l); g_app.OnResize(gW, gH); }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0); return 0;
    case WM_KEYDOWN:
        if (w == VK_ESCAPE) DestroyWindow(hWnd); break;
    }
    return DefWindowProcW(hWnd, m, w, l);
}
