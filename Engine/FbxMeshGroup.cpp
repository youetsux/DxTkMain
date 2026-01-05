// FbxMeshGroup.cpp
#include "FbxMeshGroup.h"
//
//#define DBG_LOG(fmt, ...)                          \
//do {                                               \
//    char _buf[512];                                \
//    std::snprintf(_buf, sizeof(_buf), fmt, __VA_ARGS__); \
//    OutputDebugStringA(_buf);                      \
//} while (0)

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

    // ‘S bind_vertices ‚ðär‚ß‚Ä AABB ‚ðì‚è’¼‚·iBVolume ‚Ì min/max –¼‚ÉˆË‘¶‚µ‚È‚¢j
    for (const auto& m : meshes_) {
        if (!m) continue;
        //DBG_LOG(
        //    "[Mesh] bind_vertices size = %zu\n",
        //    m->Data().bind_vertices_.size()
        //);


        const auto& bd = m->Data().bind_vertices_;
        for (const auto& v : bd) {
            bounds_.WrapBox(v.pos);
        }
    }

    bounds_.RecalcSphereFromAABB();

 /*   DBG_LOG(
        "[MeshGroup BV] min(%.3f %.3f %.3f) max(%.3f %.3f %.3f) r=%.3f finite(r=%d)\n",
        bounds_.min.x, bounds_.min.y, bounds_.min.z,
        bounds_.max.x, bounds_.max.y, bounds_.max.z,
        bounds_.radius,
        std::isfinite(bounds_.radius)
    );*/
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
