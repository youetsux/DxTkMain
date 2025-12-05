#include "EngineTime.h"
// EngineTime.cpp
#include "EngineTime.h"
#include <chrono>

namespace
{
    using Clock = std::chrono::steady_clock;

    Clock::time_point g_startTime;
    Clock::time_point g_prevTime;
    double            g_totalTime = 0.0;  // 起動からの経過時間 [秒]
    double            g_deltaTime = 0.0;  // 直近フレームの経過時間 [秒]
    double            g_fps = 60.0; // 目安用
}

void EngineTime::Reset()
{
    g_startTime = g_prevTime = Clock::now();
    g_totalTime = 0.0;
    g_deltaTime = 0.0;
    g_fps = 60.0;
}

void EngineTime::Tick()
{
    const Clock::time_point now = Clock::now();

    g_deltaTime = std::chrono::duration<double>(now - g_prevTime).count();
    g_totalTime = std::chrono::duration<double>(now - g_startTime).count();
    g_prevTime = now;

    // 安全のためのクランプを入れたければここで
    // if (g_deltaTime > 0.1) g_deltaTime = 0.1;

    if (g_deltaTime > 1e-6) {
        g_fps = 1.0 / g_deltaTime;
    }
}

double EngineTime::DeltaTime()
{
    return g_deltaTime;
}

double EngineTime::TotalTime()
{
    return g_totalTime;
}

double EngineTime::FPS()
{
    return g_fps;
}
