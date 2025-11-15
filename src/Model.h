#pragma once

#include <DirectXMath.h>
#include <string>
#include <vector>
#include <memory>

#include "Transform.h"
#include "UfbxStaticModel.h"

// RayCast 用データ（本体は別ヘッダで定義されている想定）
struct RayCastData;

namespace Model
{
    struct ModelData
    {
        bool used = false;
        std::string fileName;
        Transform transform;
        std::unique_ptr<UfbxStaticModel> ufbx;
    };

    // モジュール初期化（最大登録数を指定）
    void Initialize(size_t maxCount = 32);

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

    // UFBX モデルの一括描画
    void DrawUfbxAll(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj);

    // スケルトンの一括描画
    void DrawSkeletonAll(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj);

    // レイキャスト（中身はまだダミー）
    void RayCast(int handle, RayCastData* data);
}
