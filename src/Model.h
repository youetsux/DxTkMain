#pragma once

#include <DirectXMath.h>
#include <string>
#include <vector>
#include <memory>

#include "Transform.h"
#include "UfbxStaticModel.h"
#include "ufbx.h" // ufbx_scene / ufbx_free_scene

// RayCast 用データ（別ヘッダで定義されている想定）
struct RayCastData;

namespace Model
{
    // ---------------------------------------
    // アニメーション状態
    // ---------------------------------------
    struct AnimState
    {
        double beginTime = 0.0; // anim->time_begin
        double endTime = 0.0; // anim->time_end

        int totalFrames = 0;    // 60fps 換算の総フレーム数

        int startFrame = 0;     // 再生開始フレーム
        int endFrame = 0;     // 再生終了フレーム

        float currentFrame = 0.0f; // float で補間可能

        float speed = 1.0f;        // 1.0 = 等速
        bool  loop = true;        // ループ再生
    };

    // ---------------------------------------
    // モデル単体のデータ
    // ---------------------------------------
    struct ModelData
    {
        bool used = false;

        std::string fileName;

        Transform transform;

        std::unique_ptr<UfbxStaticModel> ufbx;

        // FBX 生シーンデータ（削除子 ufbx_free_scene）
        //std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)> scene =
        //    std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)>(nullptr, ufbx_free_scene);

        float animationFps = 60.0f; // 後で上書き

        AnimState animInfo;
    };

    // モジュール初期化（最大登録数を指定）
    void Initialize(size_t maxCount);

    // すべて破棄
    void AllRelease();

    // 単体削除
    void Delete(int handle);

    // UFBX モデル読み込み
    int LoadUfbx(const char* fbxPath);

    // Transform 設定
    void SetTransform(int handle, const Transform& transform);

    // Transform → 行列
    DirectX::XMMATRIX GetMatrix(int handle);

    // UFBX 単体描画（内部 view/proj を利用）
    void DrawUfbx(int handle);

    // Transform を都度指定して描画
    void DrawUfbx(int handle, const Transform& transform);

    // フレーム指定（基本）
    void DrawUfbx(int handle, const Transform& transform, int frame);

    // 実時間（秒）指定でアニメーションするバージョン
    void DrawUfbx(int handle, const Transform& transform, double timeSec);

    // 現在の transform を使うバージョン
    void DrawUfbx(int handle, int frame);

    // UFBX 全体描画（view/proj を内部に保存）
    void DrawUfbxAll(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj);

    // スケルトン描画（同じく内部 view/proj 利用）
    void DrawSkeletonAll();

    void SetViewProj(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj);

    // レイキャスト
    void RayCast(int handle, RayCastData* data);
}
