// EngineTime.h
#pragma once

namespace EngineTime
{
    void Reset();
    void Tick(double dtSec);

    double DeltaTime();
    double TotalTime();
}
