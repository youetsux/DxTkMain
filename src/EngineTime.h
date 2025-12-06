// EngineTime.h
#pragma once

namespace EngineTime
{
    void Reset();               // totalTime を 0 にするだけ
    void Tick(double dtSec);    // 毎フレーム or 毎Updateで呼ぶ（外から dt を渡す）

    double DeltaTime();         // 直近の dt [秒]
    double TotalTime();         // 累積時間 [秒]
}
