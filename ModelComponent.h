#pragma once
#include <string>
#include "Engine\Component.h"
#include "Engine\Model.h"      // Model::Load/Draw/Release/SetTransform/Anim API

class GameObject;

//-----------------------------------------------------------
// ModelComponent（フェーズ1：最小アニマAPI + ロード時正規化）
// 変更点(2025-12-12):
// - コンストラクタで targetHeight を受け取る（ロード時正規化用）
//   targetHeight <= 0 の場合は通常ロード（Model::Load(path)）
//   targetHeight > 0 の場合は正規化ロード（Model::Load(path, targetHeight)）
//
// 変更点(2026-01-01):
// - 「手動正規化」用に RootScale（ルートスケール）を ModelComponent から指定できるようにする
//   ※既存挙動維持のため、明示指定があるときだけ Model::SetRootScale() を呼ぶ
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
    // --- 基本 ---
    int  Handle() const { return modelHandle_; }
    bool IsLoaded() const { return modelHandle_ >= 0; }
    float TargetHeight() const { return targetHeight_; }

    // --- 手動正規化（ルートスケール） ---
    // 明示的に呼ばれたときだけ override フラグが立つ（既存の targetHeight 正規化を壊さない）
    void  SetRootScale(float rootScale);
    float GetRootScale() const;
    bool  HasRootScaleOverride() const { return hasRootScaleOverride_; }

    // --- アニメ：最小API（フェーズ1） ---
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

private:
    std::string modelPath_;
    float targetHeight_;   // ロード時正規化高さ（<=0: 無効）
    int modelHandle_;

    // 追加：手動ルートスケール
    bool  hasRootScaleOverride_;
    float rootScale_;
};
