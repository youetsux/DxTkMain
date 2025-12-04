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

}
