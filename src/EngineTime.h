// EngineTime.h
#pragma once

namespace EngineTime
{
    // 初期化（起動時／リセット時に呼ぶ）
    void Reset();

    // 毎フレーム 1 回呼ぶ（delta / total / fps を更新）
    void Tick();

    // 直近フレームの経過時間 [秒]
    double DeltaTime();

    // 起動からの累積時間 [秒]
    double TotalTime();

    // 現在の FPS（平均ではなく 1/delta ベース）
    double FPS();
}
