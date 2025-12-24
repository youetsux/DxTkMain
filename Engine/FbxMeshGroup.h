#pragma once

#include <memory>
#include <vector>

#include <DirectXMath.h>

#include "BoundingVolume.h" // BVolume
#include "FbxMesh.h"        // FbxMesh

//============================================================
// FbxMeshGroup
//  - 複数の FbxMesh（= node->mesh 単位）を束ねて扱うための薄いコンテナ
//  - この段階では既存コードに影響を与えない（どこからも参照しなければビルドはそのまま）
//
// ★変更点（新規追加ファイル）
//============================================================
class FbxMeshGroup
{
public:
    FbxMeshGroup() = default;
    ~FbxMeshGroup() = default;

    // コピー禁止（内部にGPU資源を持つため）
    FbxMeshGroup(const FbxMeshGroup&) = delete;
    FbxMeshGroup& operator=(const FbxMeshGroup&) = delete;

    // ムーブ可
    FbxMeshGroup(FbxMeshGroup&&) noexcept = default;
    FbxMeshGroup& operator=(FbxMeshGroup&&) noexcept = default;

    void Clear();

    // mesh を所有して追加
    void AddMesh(std::unique_ptr<FbxMesh> mesh);

    size_t MeshCount() const { return meshes_.size(); }
    bool   Empty() const { return meshes_.empty(); }

    // 全体バウンディング（必要なら後で使用）
    const BVolume& GetBV() const { return bounds_; }
    BVolume& GetBV() { return bounds_; }

    // 描画（SubMesh を順に描く）
    void Draw(const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj,
        FbxSkeleton& skeleton);

private:
    void RecalcBounds();

private:
    std::vector<std::unique_ptr<FbxMesh>> meshes_;
    BVolume bounds_;
};
