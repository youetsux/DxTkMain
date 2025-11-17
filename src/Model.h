#pragma once

#include <DirectXMath.h>
#include <string>
#include <vector>
#include <memory>

#include "Transform.h"
#include "UfbxStaticModel.h"
#include "ufbx.h"


// RayCast 用データ（本体は別ヘッダで定義されている想定）
struct RayCastData;

namespace Model
{

    struct AnimState
    {
        // ufbx_anim の time_begin / time_end（秒）
        double beginTime = 0.0;
        double endTime = 0.0;

        // 60fps 換算の総フレーム数
        int totalFrames = 0;

        // 再生範囲（フレーム）
        int startFrame = 0;
        int endFrame = 0;

        // 現在フレーム（float にして補間しやすく）
        float currentFrame = 0.0f;

        // 再生スピード（1.0 = 等速、2.0 = 2倍速、-1.0 = 逆再生など想定）
        float speed = 1.0f;

        // ループ再生するかどうか
        bool  loop = true;
    };

    // --- 追加ここまで ---

    struct ModelData
    {
        bool used = false;
        std::string fileName;
        Transform transform;
        std::unique_ptr<UfbxStaticModel> ufbx;
        // FBX の生シーンデータ（アニメ情報を保持）
        std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)> scene
            = std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)>(nullptr, ufbx_free_scene);

        // アニメ FPS（後で scene の anim->fps から取得する）
        float animationFps = 60.0f;
        AnimState animInfo;

    };

    // モジュール初期化（最大登録数を指定）
    void Initialize(size_t maxCount);

    // すべて破棄
    void AllRelease();

    // 単体削除
    void Delete(int handle);

    // UFBX モデル読み込み（device は内部で Gfx::Dev() を使う）
    int LoadUfbx(const char* fbxPath);

    // Transform 設定
    void SetTransform(int handle, const Transform& transform);

    // ワールド行列取得（Transform から生成／失敗時は単位行列）
    DirectX::XMMATRIX GetMatrix(int handle);

    // 単体モデルを描画（内部に保持している Transform を使用）
    void DrawUfbx(int handle);

    // Transform を引数で渡して単体描画（内部 Transform は書き換えない）
    void DrawUfbx(int handle, const Transform& transform);

    // ★ フレーム指定での描画（frame は 60fps 前提）
    void DrawUfbxAtFrame(int handle, int frame);
    void DrawUfbxAtFrame(int handle, const Transform& transform, int frame);

    // UFBX モデルの一括描画
    void DrawUfbxAll();

    // スケルトンの一括描画
    void DrawSkeletonAll();

    // レイキャスト（中身はまだダミー）
    void RayCast(int handle, RayCastData* data);
}
