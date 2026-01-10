#pragma once
#pragma once

#include <memory>

struct BakedRig;
struct BakedAnimClip;
#include <string>

#include <DirectXMath.h>
#include "ufbx.h"
#include "FbxSkeleton.h"
#include "FbxMesh.h"
#include "FbxMeshGroup.h"
#include "BoundingVolume.h"


enum class SizeMeasureAxis
{
    HeightY,      // max.y - min.y
    WidthX,       // max.x - min.x
    DepthZ,       // max.z - min.z
    MaxExtent,    // max(x,y,z)
    Radius,       // aiKvȂj
};


// ufbx O錾iwb_ɒˑȂ悤ɂj
struct ufbx_scene;
struct ufbx_anim;

//======================================================================
// FbxModel
//   - ufbx_scene ̏L
//   - FbxSkeletoni{[Aj[Vj
//   - FbxMeshibVeNX``j
// ܂Ƃ߂ĈNX
//======================================================================
class FbxModel
{
public:
    FbxModel();
    ~FbxModel();

    // ------------------------------------------------------------
    // ǂݍ݁Ej
    // ------------------------------------------------------------

    // FBX t@CǂݍŁAXPgbV\z
    bool Load(const char* fbx_path);

    // Baked import path (new, unused unless called explicitly)
    bool LoadBaked(const char* fbx_path);

    // Helper: LoadBaked() then ValidateAndDiscard() (unused unless called explicitly).
    bool LoadBakedAndDiscard(const char* fbx_path, std::string& out_error);

    // Baked data validation for scene discard (unused unless called explicitly).
    bool ValidateBakedData(std::string& out_error) const;
    void DiscardScene();
    bool ValidateAndDiscard(std::string& out_error);

    // IȃZbgiėpꍇȂǁj
    void Reset();

    // ------------------------------------------------------------
    // `
    // ------------------------------------------------------------

    // bV`iCPU XLjO݁j
    void Draw(const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj);

    // XPg̃fobO`i{[CȂǁj
    void DrawSkeleton(const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj);


    // ------------------------------------------------------------
    // Debug (Step4): Sub-mesh solo draw (multi-mesh only)
    //   -1: draw all (default)
    //  0..N-1: draw only that sub-mesh index (for visual verification)
    // ------------------------------------------------------------
    void SetDebugDrawMeshIndex(int index) { debug_draw_mesh_index_ = index; }
    int  GetDebugDrawMeshIndex() const { return debug_draw_mesh_index_; }

    // ------------------------------------------------------------
        // Aj[V
        // ------------------------------------------------------------

        // V[Ɋ܂܂uftHgAjvԂiȂ nullptrj
    const ufbx_anim* GetDefaultAnim() const;

    // ftHgAj̎ t_sec ŃXPgXV
    void UpdateSkeletonAtTime(double t_sec);

    // I anim w肵Ď t_sec ̎pɍXV
    void UpdateSkeletonAtTime(const ufbx_anim* anim, double t_sec);

    // ------------------------------------------------------------
    // ANZT
    // ------------------------------------------------------------
    const ufbx_scene* Scene()       const { return scene_.get(); }
    FbxSkeleton& Skeleton() { return skeleton_; }
    const FbxSkeleton& Skeleton()   const { return skeleton_; }
    FbxMesh& Mesh() { return mesh_; }
    const FbxMesh& Mesh()        const { return mesh_; }

    // BV ANZTiMesh ɃtH[hj
    BVolume& GetBV() { return mesh_.GetBV(); }
    const BVolume& GetBV() const { return mesh_.GetBV(); }

    // V[aANZTiSkeleton ɃtH[hj
    float SceneRadius();
    float SceneHeight();   // ǉFY imaxY - minYj
    float MeasureSize(SizeMeasureAxis axis);
    float MeasureSkinnedHeightY();

    // ------------------------------------------------------------
    // Step1: gݍݏi܂gpj
    // ------------------------------------------------------------
    FbxMeshGroup& MeshGroup() { return mesh_group_; }
    const FbxMeshGroup& MeshGroup() const { return mesh_group_; }

private:
    // V[ǂݍ݂̉
    bool LoadScene(const char* fbx_path);

private:
    // ufbx V[{́iFbxSkeleton / FbxMesh ͂QƂč\zj
    std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)> scene_{ nullptr, ufbx_free_scene };
    std::unique_ptr<BakedRig> baked_rig_;
    std::unique_ptr<BakedAnimClip> baked_anim_clip_;

    // {[Aj[V
    FbxSkeleton skeleton_;

    // bV{eNX`{`iP݊̊oHj
    FbxMesh     mesh_;

    // m[h/bVpiStep1ł͕ێ̂݁B͕ςȂj
    // Debug: draw only one sub-mesh in mesh_group_ (-1 = all)
    int debug_draw_mesh_index_ = -1;

    FbxMeshGroup mesh_group_;
    // Step2: cache init
    const ufbx_anim* last_anim_ = nullptr;
    double last_time_sec_ = -1.0;
    bool pose_dirty_ = true;

};
