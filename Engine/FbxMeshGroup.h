// FbxMeshGroup.h
#pragma once

#include <memory>
#include <vector>

#include "FbxMesh.h"
#include "BoundingVolume.h"

//======================================================================
// FbxMeshGroup



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


    FbxMesh& FirstMesh();
    const FbxMesh& FirstMesh() const;


    const std::vector<std::unique_ptr<FbxMesh>>& Meshes() const { return meshes_; }


    void Draw(const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj,
        FbxSkeleton& skeleton);


    BVolume& GetBV() { return bounds_; }
    const BVolume& GetBV() const { return bounds_; }

private:
    void RecalcBounds();

private:
    std::vector<std::unique_ptr<FbxMesh>> meshes_;
    BVolume bounds_;
};
