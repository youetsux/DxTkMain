#pragma once
#include <string>
#include <DirectXMath.h>
#include "Transform.h"   // 既存の Transform クラス

// RayCastData は別ヘッダで定義されている前提
struct RayCastData;

namespace Model
{
    void Initialize();
    int  Load(std::string fileName);
    // 追加：シーン半径を targetRadius に正規化して読み込む版
    int  Load(const std::string& fileName, float targetHeight);

    void Draw(int handle);
    void DrawSkeleton(int handle);
    void Release(int handle);
    void AllRelease();
    void SetAnimFrame(int handle, int startFrame, int endFrame, float animSpeed);
    int  GetAnimFrame(int handle);
    DirectX::XMFLOAT3 GetBonePosition(int handle, std::string boneName);
    DirectX::XMFLOAT3 GetAnimBonePosition(int handle, std::string boneName);
    void SetTransform(int handle, Transform& t);
    DirectX::XMMATRIX GetMatrix(int handle);
    void RayCast(int handle, RayCastData* data);


    // そのモデルが持つ AnimStack の数を返す
    int GetAnimStackCount(int handle);

    // AnimStack 名をインデックスで取得（0 ～ count-1）
    std::string GetAnimStackName(int handle, int index);

    // AnimStack をインデックスで指定
    void SetAnimStack(int handle, int index);

    // AnimStack を名前で指定
    void SetAnimStack(int handle, const std::string& stackName);

    // ★ 追加：アニメのポーズ制御
    void SetAnimPaused(int handle, bool paused);
    bool IsAnimPaused(int handle);

    // ★ 追加：ループ設定
    void SetAnimLoop(int handle, bool loop);
    bool IsAnimLoop(int handle);
}
