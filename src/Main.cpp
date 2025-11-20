// WinMain.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "App.h"
#include <mmsystem.h> // timeBeginPeriod / timeEndPeriod
#include <cmath>
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

    g_app.Initialize(hwnd, gW, gH); // ここは既存まま
    g_ready = true;
    ShowWindow(hwnd, SW_SHOWDEFAULT);

    timeBeginPeriod(1); // Sleep 精度向上（終了時に timeEndPeriod(1)）

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);

    const double targetDt = 1.0 / 60.0; // 60 FPS target
    const double spinThreshold = 0.003;  // 3ms のスピン閾値（必要に応じ調整）

    LARGE_INTEGER prevCnt; QueryPerformanceCounter(&prevCnt);
    double prevTime = double(prevCnt.QuadPart) / double(freq.QuadPart);

    double lastRenderTime = prevTime;
    double accumulator = 0.0;

    // FPS 表示変数
    double fpsAccum = 0.0;
    int fpsFrames = 0;
    const double fpsUpdateInterval = 0.5;
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

        // 時刻更新
        LARGE_INTEGER nowCnt; QueryPerformanceCounter(&nowCnt);
        double now = double(nowCnt.QuadPart) / double(freq.QuadPart);
        double elapsed = now - prevTime;
        prevTime = now;

        // Update 固定（accumulator ベース）
        accumulator += elapsed;
        const int maxUpdatesPerFrame = 5;
        int updates = 0;
        while (accumulator >= targetDt && updates < maxUpdatesPerFrame) {
            g_app.Update(); // 既存の Update をそのまま使用
            accumulator -= targetDt;
            ++updates;
        }
        if (updates >= maxUpdatesPerFrame) accumulator = 0.0; // 追いつけない場合は切る

        // 次の描画時刻を計算
        double nextRenderTime = lastRenderTime + targetDt;
        if (now < nextRenderTime) {
            // まだ早い → 速ければ休む（Sleep + spin）
            SleepUntil(nextRenderTime, spinThreshold, freq);
            // After SleepUntil, set now to current time
            QueryPerformanceCounter(&nowCnt);
            now = double(nowCnt.QuadPart) / double(freq.QuadPart);
        }

        // 描画（既存の g_app.Render() をそのまま呼ぶ）
        g_app.Render(); // ここは変更しない（既存実装を使用する）

        // 実描画時刻を記録
        LARGE_INTEGER afterCnt; QueryPerformanceCounter(&afterCnt);
        lastRenderTime = double(afterCnt.QuadPart) / double(freq.QuadPart);

        // FPS bookkeeping
        ++fpsFrames;
        fpsAccum += targetDt; // targetDt ベースでも良い（または実測時間を使う）
        if (fpsAccum >= fpsUpdateInterval) {
            double fps = double(fpsFrames) / fpsAccum;
            swprintf_s(titleBuf, _countof(titleBuf), L"%s - FPS: %.1f", baseTitle, fps);
            SetWindowTextW(hwnd, titleBuf);
            fpsAccum = 0.0;
            fpsFrames = 0;
        }
    }

    timeEndPeriod(1);
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT m, WPARAM w, LPARAM l)
{
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
