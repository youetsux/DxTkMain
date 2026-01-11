#pragma once
#include <string>
#include <DirectXMath.h>
#include "Transform.h"

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

    // 既存：フレーム指定再生
    void SetAnimFrame(int handle, int startFrame, int endFrame, float animSpeed);

    // 追加：現在選択中のアニメ（AnimStack/Default）をフル再生
    // ※フレーム指定再生は維持したまま、FBXの time_begin/time_end をフレームに変換して range を組む
    void SetAnimation(int handle, float animSpeed);

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

    // Root Scale / Rotation
    void  SetRootScale(int handle, float rootScale);
    float GetRootScale(int handle);

    void  SetRootRotationYawPitchRoll(int handle, float yaw, float pitch, float roll);
    void  SetRootRotationQuaternion(int handle, const DirectX::XMFLOAT4& q);
    DirectX::XMFLOAT4 GetRootRotationQuaternion(int handle);

    // degree（度）指定
    void SetRootRotationYawPitchRollDeg(int handle, float yawDeg, float pitchDeg, float rollDeg);
}
