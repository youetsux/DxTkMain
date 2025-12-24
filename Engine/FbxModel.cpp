#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "FbxModel.h"

#include <cstring>
#include <cfloat>
#include <algorithm>
#include <new> // placement new

//------------------------------------------------------------
// Step3 ύX_
// - Draw() 𕪊F
//    mesh_group_ Ȃ mesh_group_.Draw()
//    Ȃ]ʂ mesh_.Draw()
//------------------------------------------------------------

FbxModel::FbxModel()
{
}

FbxModel::~FbxModel()
{
    Reset();
}

//============================================================
// Reset
//============================================================
void FbxModel::Reset()
{
    // scene ̉
    scene_.reset(nullptr);

    //  Rs[/[uɗ炸ujč\zv
    //    iFbxMesh Rs[sȂ߁j
    skeleton_.~FbxSkeleton();
    new (&skeleton_) FbxSkeleton();

    mesh_.~FbxMesh();
    new (&mesh_) FbxMesh();

    // Step1/2/3: O[vc[cȂ
    mesh_group_.Clear();
}

//============================================================
// SceneRadius
//============================================================
float FbxModel::SceneRadius()
{
    return skeleton_.SceneRadius();
}

//============================================================
// SceneHeight
//============================================================
float FbxModel::SceneHeight()
{
    return MeasureSize(SizeMeasureAxis::HeightY);
}

//============================================================
// MeasureSize
//============================================================
float FbxModel::MeasureSize(SizeMeasureAxis axis)
{
    const BVolume& bv = mesh_.GetBV();

    float sx = (bv.max.x - bv.min.x);
    float sy = (bv.max.y - bv.min.y);
    float sz = (bv.max.z - bv.min.z);

    switch (axis)
    {
    case SizeMeasureAxis::WidthX:   return sx;
    case SizeMeasureAxis::HeightY:  return sy;
    case SizeMeasureAxis::DepthZ:   return sz;
    case SizeMeasureAxis::MaxExtent:
        return std::max(sx, std::max(sy, sz));
    case SizeMeasureAxis::Radius:
    default:
        return bv.radius;
    }
}

//============================================================
// MeasureSkinnedHeightY
//============================================================
float FbxModel::MeasureSkinnedHeightY()
{
    const auto& md = mesh_.Data();

    // XLjOʒ_LȂ炻g
    if (!md.skinned_vertices_.empty() && !md.influences_.empty() && !md.bind_vertices_.empty())
    {
        float minY = FLT_MAX;
        float maxY = -FLT_MAX;

        for (const auto& v : md.skinned_vertices_) {
            minY = std::min(minY, v.pos.y);
            maxY = std::max(maxY, v.pos.y);
        }
        return (maxY - minY);
    }

    // ܂XLjOʂi܂̓XLjȂ AABB 
    return MeasureSize(SizeMeasureAxis::HeightY);
}

//============================================================
// LoadSceneij
//============================================================
bool FbxModel::LoadScene(const char* fbx_path)
{
    ufbx_error     err;
    ufbx_load_opts opts;

    std::memset(&err, 0, sizeof(err));
    std::memset(&opts, 0, sizeof(opts));

    // DirectX Wn
    opts.target_axes = ufbx_axes_left_handed_y_up;
    opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;

    ufbx_scene* raw_scene = ufbx_load_file(fbx_path, &opts, &err);
    if (!raw_scene) {
        return false;
    }

    scene_.reset(raw_scene);
    return true;
}

//============================================================
// Load
//============================================================
bool FbxModel::Load(const char* fbx_path)
{
    Reset();

    if (!LoadScene(fbx_path)) {
        return false;
    }

    const ufbx_scene* scene = scene_.get();
    if (!scene) {
        return false;
    }

    // Skeleton \z
    if (!skeleton_.BuildFromScene(scene)) {
        return false;
    }

    // ------------------------------------------------------------
    // Step2: m[h/bV
    //   - node->mesh m[h𐔂
    // ------------------------------------------------------------
    size_t mesh_node_count = 0;
    for (size_t i = 0; i < scene->nodes.count; ++i)
    {
        const ufbx_node* node = scene->nodes.data[i];
        if (!node) continue;
        if (!node->mesh) continue;

        // 󃁃bV͏OiÔ߁j
        const ufbx_mesh* m = node->mesh;
        if (m->num_faces == 0) continue;

        ++mesh_node_count;
    }

    // ------------------------------------------------------------
    // ݊ێ: ]̒PꃁbVoH͕K\z
    // ------------------------------------------------------------
    if (!mesh_.BuildFromScene(scene, skeleton_, fbx_path)) {
        return false;
    }

    // ------------------------------------------------------------
    // Step2: bVȂAǉ group \zĂ
    // ------------------------------------------------------------
    if (mesh_node_count >= 2)
    {
        mesh_group_.Clear();

        for (size_t i = 0; i < scene->nodes.count; ++i)
        {
            const ufbx_node* node = scene->nodes.data[i];
            if (!node) continue;
            if (!node->mesh) continue;

            const ufbx_mesh* m = node->mesh;
            if (m->num_faces == 0) continue;

            std::unique_ptr<FbxMesh> sub = std::make_unique<FbxMesh>();
            if (!sub->BuildFromNode(scene, node, skeleton_, fbx_path))
            {
                return false;
            }

            mesh_group_.AddMesh(std::move(sub));
        }

        if (mesh_group_.Empty()) {
            return false;
        }
    }

    // p
    UpdateSkeletonAtTime(0.0);
    return true;
}

//============================================================
// GetDefaultAnim
//============================================================
const ufbx_anim* FbxModel::GetDefaultAnim() const
{
    const ufbx_scene* scene = scene_.get();
    if (!scene) return nullptr;

    return scene->anim;
}

//============================================================
// UpdateSkeletonAtTimeiftHgAjj
//============================================================
void FbxModel::UpdateSkeletonAtTime(double t_sec)
{
    const ufbx_scene* scene = scene_.get();
    const ufbx_anim* anim = GetDefaultAnim();
    skeleton_.UpdateAtTime(scene, anim, t_sec);
}

//============================================================
// UpdateSkeletonAtTimeiAjj
//============================================================
void FbxModel::UpdateSkeletonAtTime(const ufbx_anim* anim, double t_sec)
{
    const ufbx_scene* scene = scene_.get();
    skeleton_.UpdateAtTime(scene, anim, t_sec);
}

//============================================================
// DrawiCPU XLjO{`j
//  Step3: group  group `
//============================================================
void FbxModel::Draw(
    const DirectX::XMMATRIX& world,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& proj)
{
    if (!mesh_group_.Empty())
    {
        const int idx = debug_draw_mesh_index_;
        if (idx >= 0 && idx < (int)mesh_group_.MeshCount())
        {
            // Step4: solo draw
            mesh_group_.Meshes()[(size_t)idx]->Draw(world, view, proj, skeleton_);
        }
        else
        {
            // default: draw all
            mesh_group_.Draw(world, view, proj, skeleton_);
        }
        return;
    }

    mesh_.Draw(world, view, proj, skeleton_);
}

//============================================================
// DrawSkeletonifobOj
//============================================================
void FbxModel::DrawSkeleton(
    const DirectX::XMMATRIX& world,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& proj)
{
    skeleton_.DrawDebug(world, view, proj);
}
