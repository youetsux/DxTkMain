// FbxMeshGroup.cpp
#include "FbxMeshGroup.h"

void FbxMeshGroup::Clear()
{
    meshes_.clear();
    bounds_.Reset();
}

void FbxMeshGroup::AddMesh(std::unique_ptr<FbxMesh> mesh)
{
    if (!mesh) return;
    meshes_.push_back(std::move(mesh));
    RecalcBounds();
}

FbxMesh& FbxMeshGroup::FirstMesh()
{
    return *meshes_.front();
}

const FbxMesh& FbxMeshGroup::FirstMesh() const
{
    return *meshes_.front();
}

void FbxMeshGroup::RecalcBounds()
{
    bounds_.Reset();

    // 全 bind_vertices を舐めて AABB を作り直す（BVolume の min/max 名に依存しない）
    for (const auto& m : meshes_) {
        if (!m) continue;

        const auto& bd = m->Data().bind_vertices_;
        for (const auto& v : bd) {
            bounds_.WrapBox(v.pos);
        }
    }

    bounds_.RecalcSphereFromAABB();

}

void FbxMeshGroup::Draw(const DirectX::XMMATRIX& world,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& proj,
    FbxSkeleton& skeleton)
{
    for (auto& m : meshes_) {
        if (!m) continue;
        m->Draw(world, view, proj, skeleton);
    }
}
