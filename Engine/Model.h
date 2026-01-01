#pragma once
#include <string>
#include <DirectXMath.h>
#include "Transform.h"   // 依存の Transform クラス

// RayCastData は別ヘッダーで定義されている前提
struct RayCastData;

namespace Model
{
    void Initialize();
    int  Load(std::string fileName);
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

    int GetAnimStackCount(int handle);
    std::string GetAnimStackName(int handle, int index);
    void SetAnimStack(int handle, int index);
    void SetAnimStack(int handle, const std::string& stackName);

    void SetAnimPaused(int handle, bool paused);
    bool IsAnimPaused(int handle);

    void SetAnimLoop(int handle, bool loop);
    bool IsAnimLoop(int handle);

    // ------------------------------------------------------------
    // 追加：ルートスケール（手動正規化用）
    // world = R_root * S_root * TransformWorld の順で適用される
    // ------------------------------------------------------------
    void  SetRootScale(int handle, float rootScale);
    float GetRootScale(int handle);

    // ------------------------------------------------------------
    // 追加：ルートローテーション（手動正規化用）
    // world = R_root * S_root * TransformWorld
    // - プリマルチなので平行移動も回転される
    // - 単位はラジアン（Yaw/Pitch/Roll）
    // ------------------------------------------------------------
    void  SetRootRotationYawPitchRoll(int handle, float yaw, float pitch, float roll);
    void  SetRootRotationQuaternion(int handle, const DirectX::XMFLOAT4& q);
    DirectX::XMFLOAT4 GetRootRotationQuaternion(int handle);
    // degree（度）指定版
    void SetRootRotationYawPitchRollDeg(int handle, float yawDeg, float pitchDeg, float rollDeg);

}
