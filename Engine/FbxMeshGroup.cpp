#include "FbxMeshGroup.h"

#include <cfloat>

//============================================================
// Clear
//============================================================
void FbxMeshGroup::Clear()
{
    meshes_.clear();
    bounds_.Reset();
}

//============================================================
// AddMesh
//============================================================
void FbxMeshGroup::AddMesh(std::unique_ptr<FbxMesh> mesh)
{
    if (!mesh) return;
    meshes_.push_back(std::move(mesh));
    RecalcBounds();
}

//============================================================
// RecalcBounds
//  - グループ全体の bounds_ を作り直す
//  - BVolume の API（Reset/WrapBox/RecalcSphereFromAABB）だけ使用
//============================================================
void FbxMeshGroup::RecalcBounds()
{
    bounds_.Reset();

    // 既存の FbxMesh が持つ MeshData を見て bounds を作る
    for (size_t i = 0; i < meshes_.size(); ++i)
    {
        FbxMesh* m = meshes_[i].get();
        if (!m) continue;

        const auto& md = m->Data();

        // bind_vertices_ があればそれを優先（スキン有りでもバインドは常にある想定）
        const auto& verts =
            (!md.bind_vertices_.empty()) ? md.bind_vertices_ : md.vertices_;

        for (size_t v = 0; v < verts.size(); ++v)
        {
            bounds_.WrapBox(verts[v].pos);
        }
    }

    bounds_.RecalcSphereFromAABB();
}

//============================================================
// Draw
//============================================================
void FbxMeshGroup::Draw(const DirectX::XMMATRIX& world,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& proj,
    FbxSkeleton& skeleton)
{
    for (size_t i = 0; i < meshes_.size(); ++i)
    {
        FbxMesh* m = meshes_[i].get();
        if (!m) continue;

        m->Draw(world, view, proj, skeleton);
    }
}
