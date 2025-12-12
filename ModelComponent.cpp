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
{
}

ModelComponent::~ModelComponent()
{
    Release();
}

void ModelComponent::Initialize()
{
    if (modelHandle_ >= 0) return;

    // 変更点(2025-12-12):
    // - targetHeight_ によってロード関数を切り替える
    if (targetHeight_ > 0.0f)
    {
        modelHandle_ = Model::Load(modelPath_, targetHeight_);
    }
    else
    {
        modelHandle_ = Model::Load(modelPath_);
    }

    // Owner の Transform を Model に接続
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
// アニメ：最小API（フェーズ1）
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
