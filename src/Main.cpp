// Main.cpp  (方針A：可変dtに統一 / Updateは1回/フレーム)
// - EngineTime::Tick(elapsed) は 1回/フレーム（可変dt）
// - g_app.Update() も 1回/フレーム
// - Render は 60Hz 目標で SleepUntil によるペーシングは残す（dt自体は可変のまま）
//
// 表示（1秒平均）
//   RenderFPS : ループ全体（Update + Wait + Render）
//   UpdateFPS : Update開始間隔（前回Update開始→今回Update開始）
//   upd/rnd/wait(ms) : それぞれの平均時間（犯人特定用）

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>   // timeBeginPeriod / timeEndPeriod
#include <algorithm>    // std::max
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

    const double invFreq = 1.0 / double(freq.QuadPart);

    // Render の目標間隔（ペーシング用）。dt は可変のまま（elapsed を Tick する）
    const double targetDt = 1.0 / 60.0;
    const double spinThreshold = 0.003;

    // Update開始時刻（可変dtの基準：Sleep/Renderも含む“本当のUpdate間隔”）
    LARGE_INTEGER prevUpdateCnt; QueryPerformanceCounter(&prevUpdateCnt);
    double prevUpdateTime = double(prevUpdateCnt.QuadPart) * invFreq;

    // ペーシング用（「実際に Render し終えた時刻」を記録）
    double lastRenderTime = prevUpdateTime;

    // FPS 表示変数（1秒平均）
    double fpsTimeAccum = 0.0;
    int    fpsFrames = 0;

    // UpdateFPS 表示変数（Update開始間隔で計測）
    double updTimeAccum = 0.0;
    int    updFrames = 0;

    // プロファイル（平均ms）
    double updMsAccum = 0.0;
    double rndMsAccum = 0.0;
    double waitMsAccum = 0.0;
    int    profFrames = 0;

    const double fpsUpdateInterval = 1.0;   // 1秒ごとの平均
    wchar_t titleBuf[256];
    const wchar_t* baseTitle = L"MyApp";

    MSG msg{};
    bool run = true;
    while (run)
    {
        // ループ全体(RenderFPS)計測：QPC差分
        LARGE_INTEGER frameBeginCnt; QueryPerformanceCounter(&frameBeginCnt);

        // メッセージ処理
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { run = false; break; }
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        if (!run) break;

        // ---- Update開始時刻で “本当のUpdate間隔” を取る（Sleep/Render含む） ----
        LARGE_INTEGER updBeginCnt; QueryPerformanceCounter(&updBeginCnt);
        double updBegin = double(updBeginCnt.QuadPart) * invFreq;

        double elapsed = updBegin - prevUpdateTime;   // ★Update呼び出し間隔（Sleep/Render含む）
        prevUpdateTime = updBegin;

        // ★ EngineTime は可変dtで更新（1フレームに1回だけ）
        EngineTime::Tick(elapsed);

        // Update 計測
        LARGE_INTEGER u0; QueryPerformanceCounter(&u0);
        g_app.Update();
        LARGE_INTEGER u1; QueryPerformanceCounter(&u1);
        double updMs = double(u1.QuadPart - u0.QuadPart) * invFreq * 1000.0;
        updMsAccum += updMs;

        // ★ UpdateFPS（= 1/平均elapsed）
        updFrames++;
        updTimeAccum += elapsed;

        // ---- 60Hz目標のペーシング（dtは変えない） ----
        LARGE_INTEGER nowCnt; QueryPerformanceCounter(&nowCnt);
        double now = double(nowCnt.QuadPart) * invFreq;

        double nextRenderTime = lastRenderTime + targetDt;

        // Wait（SleepUntil）計測
        double waitMs = 0.0;
        if (now < nextRenderTime) {
            LARGE_INTEGER w0; QueryPerformanceCounter(&w0);
            SleepUntil(nextRenderTime, spinThreshold, freq);
            LARGE_INTEGER w1; QueryPerformanceCounter(&w1);
            waitMs = double(w1.QuadPart - w0.QuadPart) * invFreq * 1000.0;
        }
        waitMsAccum += waitMs;

        // Render 計測
        LARGE_INTEGER r0; QueryPerformanceCounter(&r0);
        g_app.Render();
        LARGE_INTEGER r1; QueryPerformanceCounter(&r1);
        double rndMs = double(r1.QuadPart - r0.QuadPart) * invFreq * 1000.0;
        rndMsAccum += rndMs;

        // 実描画時刻を記録（次フレームのペーシング基準）
        LARGE_INTEGER afterCnt; QueryPerformanceCounter(&afterCnt);
        lastRenderTime = double(afterCnt.QuadPart) * invFreq;

        // ---- RenderFPS計測（実フレーム時間：Update+Wait+Render を含む） ----
        double frameDt = double(afterCnt.QuadPart - frameBeginCnt.QuadPart) * invFreq;
        fpsFrames++;
        fpsTimeAccum += frameDt;

        profFrames++;

        if (fpsTimeAccum >= fpsUpdateInterval) {
            double renderFps = double(fpsFrames) / fpsTimeAccum;
            double updateFps = (updTimeAccum > 0.0) ? double(updFrames) / updTimeAccum : 0.0;

            double avgUpdMs = (profFrames > 0) ? (updMsAccum / double(profFrames)) : 0.0;
            double avgRndMs = (profFrames > 0) ? (rndMsAccum / double(profFrames)) : 0.0;
            double avgWaitMs = (profFrames > 0) ? (waitMsAccum / double(profFrames)) : 0.0;

            swprintf_s(titleBuf, _countof(titleBuf),
                L"%s - R:%.1f U:%.1f  upd:%.1fms rnd:%.1fms wait:%.1fms",
                baseTitle, renderFps, updateFps, avgUpdMs, avgRndMs, avgWaitMs);
            SetWindowTextW(hwnd, titleBuf);

            fpsTimeAccum = 0.0; fpsFrames = 0;
            updTimeAccum = 0.0; updFrames = 0;

            updMsAccum = 0.0;
            rndMsAccum = 0.0;
            waitMsAccum = 0.0;
            profFrames = 0;
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
