// FbxMeshGroup.h
#pragma once

#include <memory>
#include <vector>


#include "BoundingVolume.h"


class FbxMesh;
class FbxSkeleton;
//======================================================================
// FbxMeshGroup
//   - FBX 内で「ノードごとに分割されているメッシュ」をまとめて扱うための入れ物
//   - Draw() は全メッシュを順に描画する（同一スケルトン前提）
//   - bounds_ は全メッシュ合算の BV（AABB/Sphere）
//======================================================================
class FbxMeshGroup
{
public:
    FbxMeshGroup() = default;
    ~FbxMeshGroup() = default;

    void Clear();
    void AddMesh(std::unique_ptr<FbxMesh> mesh);

    bool Empty() const { return meshes_.empty(); }
    size_t MeshCount() const { return meshes_.size(); }

    // 互換用: 先頭メッシュ参照（単一メッシュ時の既存コードを壊しにくくする）
    FbxMesh& FirstMesh();
    const FbxMesh& FirstMesh() const;

    // FbxModel から内部を走査したい時用
    const std::vector<std::unique_ptr<FbxMesh>>& Meshes() const { return meshes_; }

    // 描画（全メッシュ）
    void Draw(const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj,
        FbxSkeleton& skeleton);

    // BV（全メッシュ合算）
    BVolume& GetBV() { return bounds_; }
    const BVolume& GetBV() const { return bounds_; }

private:
    void RecalcBounds();

private:
    std::vector<std::unique_ptr<FbxMesh>> meshes_;
    BVolume bounds_;
};
