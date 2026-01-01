#include "ModelComponent.h"
#include "Engine\GameObject.h"
#include "Engine\Transform.h"

ModelComponent::ModelComponent(GameObject* owner,
    const std::string& modelPath,
    float targetHeight)
    : Component(owner)
    , modelPath_(modelPath)
    , targetHeight_(targetHeight)
    , modelHandle_(-1)
    , hasRootScaleOverride_(false)
    , rootScale_(1.0f)
    , hasRootRotationOverride_(false)
    , rootRotationQ_(DirectX::XMFLOAT4(0, 0, 0, 1))
{
}

ModelComponent::~ModelComponent()
{
    Release();
}

void ModelComponent::Initialize()
{
    if (modelHandle_ >= 0) return;

    if (targetHeight_ > 0.0f)
    {
        modelHandle_ = Model::Load(modelPath_, targetHeight_);
    }
    else
    {
        modelHandle_ = Model::Load(modelPath_);
    }

    if (modelHandle_ < 0) return;

    // 手動ルートスケール（明示指定があるときだけ）
    if (hasRootScaleOverride_)
    {
        Model::SetRootScale(modelHandle_, rootScale_);
    }

    // 手動ルートローテーション（明示指定があるときだけ）
    if (hasRootRotationOverride_)
    {
        Model::SetRootRotationQuaternion(modelHandle_, rootRotationQ_);
    }

    Model::SetTransform(modelHandle_, owner_->GetTransform());
}

void ModelComponent::Draw()
{
    if (modelHandle_ >= 0)
    {
        Model::Draw(modelHandle_);
    }
}

void ModelComponent::Release()
{
    if (modelHandle_ >= 0)
    {
        Model::Release(modelHandle_);
        modelHandle_ = -1;
    }
}

//-----------------------------------------------------------
// 手動ルートスケール
//-----------------------------------------------------------
void ModelComponent::SetRootScale(float rootScale)
{
    hasRootScaleOverride_ = true;
    rootScale_ = rootScale;

    if (modelHandle_ >= 0)
    {
        Model::SetRootScale(modelHandle_, rootScale_);
    }
}

float ModelComponent::GetRootScale() const
{
    if (modelHandle_ >= 0)
    {
        return Model::GetRootScale(modelHandle_);
    }
    return rootScale_;
}

//-----------------------------------------------------------
// 手動ルートローテーション
//-----------------------------------------------------------
void ModelComponent::SetRootRotationYawPitchRoll(float yaw, float pitch, float roll)
{
    hasRootRotationOverride_ = true;

    DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
    DirectX::XMStoreFloat4(&rootRotationQ_, q);

    if (modelHandle_ >= 0)
    {
        Model::SetRootRotationQuaternion(modelHandle_, rootRotationQ_);
    }
}

void ModelComponent::SetRootRotationQuaternion(const DirectX::XMFLOAT4& q)
{
    hasRootRotationOverride_ = true;
    rootRotationQ_ = q;

    if (modelHandle_ >= 0)
    {
        Model::SetRootRotationQuaternion(modelHandle_, rootRotationQ_);
    }
}

DirectX::XMFLOAT4 ModelComponent::GetRootRotationQuaternion() const
{
    if (modelHandle_ >= 0)
    {
        return Model::GetRootRotationQuaternion(modelHandle_);
    }
    return rootRotationQ_;
}

//-----------------------------------------------------------
// アニメ：最小API
//-----------------------------------------------------------
void ModelComponent::SetAnimRange(int startFrame, int endFrame, float animSpeed)
{
    if (modelHandle_ < 0) return;
    Model::SetAnimFrame(modelHandle_, startFrame, endFrame, animSpeed);
}

void ModelComponent::Play(int startFrame, int endFrame, float animSpeed, bool loop)
{
    if (modelHandle_ < 0) return;

    Model::SetAnimFrame(modelHandle_, startFrame, endFrame, animSpeed);
    Model::SetAnimLoop(modelHandle_, loop);
    Model::SetAnimPaused(modelHandle_, false);
}

void ModelComponent::SetLoop(bool loop)
{
    if (modelHandle_ < 0) return;
    Model::SetAnimLoop(modelHandle_, loop);
}

bool ModelComponent::IsLoop() const
{
    if (modelHandle_ < 0) return true;
    return Model::IsAnimLoop(modelHandle_);
}

void ModelComponent::SetPaused(bool paused)
{
    if (modelHandle_ < 0) return;
    Model::SetAnimPaused(modelHandle_, paused);
}

bool ModelComponent::IsPaused() const
{
    if (modelHandle_ < 0) return false;
    return Model::IsAnimPaused(modelHandle_);
}

int ModelComponent::GetCurrentFrame() const
{
    if (modelHandle_ < 0) return 0;
    return Model::GetAnimFrame(modelHandle_);
}

int ModelComponent::GetAnimStackCount() const
{
    if (modelHandle_ < 0) return 0;
    return Model::GetAnimStackCount(modelHandle_);
}

std::string ModelComponent::GetAnimStackName(int index) const
{
    if (modelHandle_ < 0) return {};
    return Model::GetAnimStackName(modelHandle_, index);
}

void ModelComponent::SetAnimStack(int index)
{
    if (modelHandle_ < 0) return;
    Model::SetAnimStack(modelHandle_, index);
}

void ModelComponent::SetAnimStack(const std::string& stackName)
{
    if (modelHandle_ < 0) return;
    Model::SetAnimStack(modelHandle_, stackName);
}

void ModelComponent::SetRootRotationYawPitchRollDeg(float yawDeg, float pitchDeg, float rollDeg)
{
    hasRootRotationOverride_ = true;

    const float DEG2RAD = DirectX::XM_PI / 180.0f;

    const float yaw = yawDeg * DEG2RAD;
    const float pitch = pitchDeg * DEG2RAD;
    const float roll = rollDeg * DEG2RAD;

    DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
    DirectX::XMStoreFloat4(&rootRotationQ_, q);

    if (modelHandle_ >= 0)
    {
        Model::SetRootRotationQuaternion(modelHandle_, rootRotationQ_);
    }
}
