#pragma once

#include <DirectXMath.h>
#include <string>
#include <vector>
#include <memory>

#include "Transform.h"
#include "FbxModel.h"   // ★ 旧 "Fbx.h" の代わり

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
        std::string fileName;

        // 旧：Fbx* pFbx;
        // 同一 FBX を複数ハンドルで共有したいので shared_ptr 推奨
        std::shared_ptr<FbxModel> pFbx;

        Transform transform;

        float nowFrame, animSpeed;
        int   startFrame, endFrame;

        ModelData()
            : pFbx(), nowFrame(0), animSpeed(0), startFrame(0), endFrame(0)
        {
        }

        void SetAnimFrame(int start, int end, float speed)
        {
            nowFrame = static_cast<float>(start);
            startFrame = start;
            endFrame = end;
            animSpeed = speed;
        }
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
