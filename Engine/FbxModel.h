#pragma once
#pragma once

#include <memory>
#include <vector>

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
    Radius,
};


struct ufbx_scene;
struct ufbx_anim;

//======================================================================
// FbxModel
//   - FbxSkeletoni{[Aj[Vj
//   - FbxMeshibVeNX``j
//======================================================================
class FbxModel
{
public:
    FbxModel();
    ~FbxModel();

    // ------------------------------------------------------------
    // ------------------------------------------------------------

    bool Load(const char* fbx_path);

    // Baked import path (new, unused unless called explicitly)
    bool LoadBaked(const char* fbx_path);

    // Helper: LoadBaked() then ValidateAndDiscard() (unused unless called explicitly).
    bool LoadBakedAndDiscard(const char* fbx_path, std::string& out_error);

    // Helper: LoadBaked() then validate. If validate_transforms is true, compare baked runtime
    // world transforms against ufbx evaluation before discarding the scene.
    bool LoadBakedAndDiscardEx(const char* fbx_path, std::string& out_error, bool validate_transforms);

    // Baked data validation for scene discard (unused unless called explicitly).
    bool ValidateBakedData(std::string& out_error) const;
    // Optional: Validate that baked runtime world transforms match ufbx evaluation.
    // This requires scene_ to still be alive (call before DiscardScene()).
    bool ValidateBakedTransforms(std::string& out_error) const;
    void DiscardScene();
    bool ValidateAndDiscard(std::string& out_error);

    const std::string& GetLastError() const;

    const BakedRig* GetBakedRig() const { return baked_rig_.get(); }
    // Active baked clip selected by SetAnimStack().
    // If the FBX has no anim stacks, this returns the baked default anim (scene->anim) if available.
    const BakedAnimClip* GetBakedAnimClip() const;

    // Anim stacks (works even after DiscardScene())
    int GetAnimStackCount() const;
    std::string GetAnimStackName(int index) const;
    bool SetAnimStack(int index);
    bool SetAnimStack(const std::string& stackName);

    // scene を discard した後でも、モデルサイズ推定などに使うため保持する。
    // ufbx の UFBX_COORDINATE_AXIS_* の値をそのまま返す。
    int GetBakedUpAxis() const { return baked_up_axis_; }

    void Reset();

    // ------------------------------------------------------------
    // `
    // ------------------------------------------------------------

    void Draw(const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj);

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

    const ufbx_anim* GetDefaultAnim() const;

    void UpdateSkeletonAtTime(double t_sec);

    void UpdateSkeletonAtTime(const ufbx_anim* anim, double t_sec);

    // ------------------------------------------------------------
    // ANZT
    // ------------------------------------------------------------
    const ufbx_scene* Scene()       const { return scene_.get(); }
    FbxSkeleton& Skeleton() { return skeleton_; }
    const FbxSkeleton& Skeleton()   const { return skeleton_; }
    FbxMesh& Mesh() { return mesh_; }
    const FbxMesh& Mesh()        const { return mesh_; }

    BVolume& GetBV() { return mesh_.GetBV(); }
    const BVolume& GetBV() const { return mesh_.GetBV(); }

    float SceneRadius();
    float SceneHeight();
    float MeasureSize(SizeMeasureAxis axis);
    float MeasureSkinnedHeightY();

    // ------------------------------------------------------------
    // ------------------------------------------------------------
    FbxMeshGroup& MeshGroup() { return mesh_group_; }
    const FbxMeshGroup& MeshGroup() const { return mesh_group_; }

private:
    bool LoadScene(const char* fbx_path);

private:
    std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)> scene_{ nullptr, ufbx_free_scene };
    std::unique_ptr<BakedRig> baked_rig_;
    struct BakedAnimStack
    {
        std::string name;
        std::unique_ptr<BakedAnimClip> clip;
    };

    // All stacks baked at import time. The active stack can be switched after DiscardScene().
    std::vector<BakedAnimStack> baked_anim_stacks_;
    int baked_active_stack_index_ = -1;

    // If there are no anim stacks, we still bake scene->anim here.
    std::unique_ptr<BakedAnimClip> baked_default_clip_;

    // scene->settings.axes.up を LoadScene()/LoadBaked() 時に保存しておく。
    // scene が discard 済みでも参照できる。
    int baked_up_axis_ = 0;

    std::string last_error_;

    // {[Aj[V
    FbxSkeleton skeleton_;

    FbxMesh     mesh_;

    // Debug: draw only one sub-mesh in mesh_group_ (-1 = all)
    int debug_draw_mesh_index_ = -1;

    FbxMeshGroup mesh_group_;
    // Step2: cache init
    const ufbx_anim* last_anim_ = nullptr;
    double last_time_sec_ = -1.0;
    bool pose_dirty_ = true;

};
