#pragma once

#include <DirectXMath.h>
#include <string>
#include <vector>
#include <memory>

#include "Transform.h"
#include "UfbxStaticModel.h"

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

    // --------------------------------------------------------
    // 初期化／破棄
    // --------------------------------------------------------
    void Initialize(size_t maxCount = 32);
    void AllRelease();
    void Delete(int handle);

    // --------------------------------------------------------
    // ロード（デバイスは内部で Gfx::Dev() を使う）
    // --------------------------------------------------------
    int LoadUfbx(const char* fbxPath);

    // --------------------------------------------------------
    // トランスフォーム
    // --------------------------------------------------------
    void SetTransform(int handle, Transform& transform);
    DirectX::XMMATRIX GetMatrix(int handle);

    // --------------------------------------------------------
    // 描画（ctx / states / fx は内部で取得）
    // --------------------------------------------------------
    void DrawUfbxAll(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj);
    void DrawSkeletonAll(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj);

    // --------------------------------------------------------
    // レイキャスト（必要な場合）
    // --------------------------------------------------------
    void RayCast(int handle, RayCastData* data);
}
