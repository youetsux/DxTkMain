#pragma once
#include <string>
#include <DirectXMath.h>
#include "Engine\Component.h"
#include "Engine\Model.h"

class GameObject;

//-----------------------------------------------------------
// ModelComponent


//-----------------------------------------------------------
class ModelComponent : public Component
{
public:
    ModelComponent(GameObject* owner,
        const std::string& modelPath,
        float targetHeight = 0.0f);
    ModelComponent(GameObject* owner,
        const std::string& modelPath,
        bool useBaked,
        float targetHeight = 0.0f);
    ~ModelComponent() override;

    void Initialize() override;
    void Draw() override;
    void Release() override;

public:
    int  Handle() const { return modelHandle_; }
    bool IsLoaded() const { return modelHandle_ >= 0; }
    float TargetHeight() const { return targetHeight_; }


    void  SetRootScale(float rootScale);
    float GetRootScale() const;
    bool  HasRootScaleOverride() const { return hasRootScaleOverride_; }



    void SetRootRotationYawPitchRoll(float yaw, float pitch, float roll);
    void SetRootRotationQuaternion(const DirectX::XMFLOAT4& q);
    DirectX::XMFLOAT4 GetRootRotationQuaternion() const;
    bool HasRootRotationOverride() const { return hasRootRotationOverride_; }


    void SetRootRotationYawPitchRollDeg(float yawDeg, float pitchDeg, float rollDeg);


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


    void SetAnimation(float animSpeed, bool loop);

private:
    std::string modelPath_;
    float targetHeight_;
    int modelHandle_;
    bool useBaked_;

    bool  hasRootScaleOverride_;
    float rootScale_;

    bool  hasRootRotationOverride_;
    DirectX::XMFLOAT4 rootRotationQ_;
};
