#pragma once
#include <string>
#include <DirectXMath.h>
#include "Engine\Component.h"
#include "Engine\Model.h"

class GameObject;

//-----------------------------------------------------------
// ModelComponent
// - targetHeight による自動スケールは維持
// - 手動「RootScale」「RootRotation」も指定可能（明示指定時のみ上書き）
//-----------------------------------------------------------
class ModelComponent : public Component
{
public:
    ModelComponent(GameObject* owner,
        const std::string& modelPath,
        float targetHeight = 0.0f);
    ~ModelComponent() override;

    void Initialize() override;
    void Draw() override;
    void Release() override;

public:
    int  Handle() const { return modelHandle_; }
    bool IsLoaded() const { return modelHandle_ >= 0; }
    float TargetHeight() const { return targetHeight_; }

    // --- 手動ルートスケール ---
    void  SetRootScale(float rootScale);
    float GetRootScale() const;
    bool  HasRootScaleOverride() const { return hasRootScaleOverride_; }

    // --- 手動ルートローテーション ---
    // 単位はラジアン
    void SetRootRotationYawPitchRoll(float yaw, float pitch, float roll);
    void SetRootRotationQuaternion(const DirectX::XMFLOAT4& q);
    DirectX::XMFLOAT4 GetRootRotationQuaternion() const;
    bool HasRootRotationOverride() const { return hasRootRotationOverride_; }

    // --- アニメ：最小API ---
    void SetAnimRange(int startFrame, int endFrame, float animSpeed);
    void Play(int startFrame, int endFrame, float animSpeed, bool loop);

    void SetLoop(bool loop);
    bool IsLoop() const;

    void SetPaused(bool paused);
    bool IsPaused() const;

    int GetCurrentFrame() const;

    int  GetAnimStackCount() const;
    std::string GetAnimStackName(int index) const;
    void SetAnimStack(int index);
    void SetAnimStack(const std::string& stackName);
    // degree（度）指定
    void SetRootRotationYawPitchRollDeg(float yawDeg, float pitchDeg, float rollDeg);

private:
    std::string modelPath_;
    float targetHeight_;
    int modelHandle_;

    bool  hasRootScaleOverride_;
    float rootScale_;

    bool  hasRootRotationOverride_;
    DirectX::XMFLOAT4 rootRotationQ_;
};
