// EngineTime.cpp
#include "EngineTime.h"

namespace
{
    double g_deltaTime = 0.0;
    double g_totalTime = 0.0;
}

void EngineTime::Reset()
{
    g_deltaTime = 0.0;
    g_totalTime = 0.0;
}

void EngineTime::Tick(double dtSec)
{
    g_deltaTime = dtSec;
    g_totalTime += dtSec;
}

double EngineTime::DeltaTime()
{
    return g_deltaTime;
}

double EngineTime::TotalTime()
{
    return g_totalTime;
}
