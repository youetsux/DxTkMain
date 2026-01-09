#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "FbxModel.h"
#include "BakedRig.h"
#include "BakedAnim.h"
#include "BakedMeshImporter.h"

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
    // Step2: cache init
    last_anim_ = nullptr;
    last_time_sec_ = -1.0;
    pose_dirty_ = true;
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
// LoadBaked (new path, unused unless called explicitly)
//  - Uses BakedMeshImporter to build meshes, then feeds into existing FbxMesh/FbxMeshGroup.
//  - Does NOT change existing Load() behavior.
//============================================================
bool FbxModel::LoadBaked(const char* fbx_path)
{
    Reset();

    if (!LoadScene(fbx_path)) {
        return false;
    }

    const ufbx_scene* scene = scene_.get();
    if (!scene) {
        return false;
    }

    // Skeleton (same as normal path)
    if (!skeleton_.BuildFromScene(scene)) {
        return false;
    }

    // StepA-3: Extract rest local transforms for all nodes into baked_rig_ (unused yet)
    baked_rig_ = std::make_unique<BakedRig>();
    baked_rig_->nodes.clear();
    baked_rig_->nodes.resize(scene->nodes.count);

    // element_id values are indexes into scene->elements
    baked_rig_->element_id_to_node.clear();
    baked_rig_->element_id_to_node.resize(scene->elements.count, 0xFFFFFFFFu);

    for (size_t i = 0; i < scene->nodes.count; ++i)
    {
        const ufbx_node* node = scene->nodes.data[i];
        if (!node) continue;

        uint32_t node_index = node->typed_id;
        if (node_index >= baked_rig_->nodes.size()) continue;

        BakedRigNode& dst = baked_rig_->nodes[node_index];
        dst.element_id = node->element_id;

        if (node->element_id < baked_rig_->element_id_to_node.size()) {
            baked_rig_->element_id_to_node[node->element_id] = node_index;
        }

        dst.parent = node->parent ? node->parent->typed_id : 0xFFFFFFFFu;

        const ufbx_transform& lt = node->local_transform;
        dst.t[0] = (float)lt.translation.x;
        dst.t[1] = (float)lt.translation.y;
        dst.t[2] = (float)lt.translation.z;

        dst.r[0] = (float)lt.rotation.x;
        dst.r[1] = (float)lt.rotation.y;
        dst.r[2] = (float)lt.rotation.z;
        dst.r[3] = (float)lt.rotation.w;

        dst.s[0] = (float)lt.scale.x;
        dst.s[1] = (float)lt.scale.y;
        dst.s[2] = (float)lt.scale.z;
    }

    // StepB-3: Bake first available animation into baked_anim_clip_ (unused yet)
    baked_anim_clip_.reset();

    const ufbx_anim* bake_anim = nullptr;
    if (scene->anim_stacks.count > 0) {
        const ufbx_anim_stack* st = scene->anim_stacks.data[0];
        if (st) bake_anim = st->anim;
    }
    if (!bake_anim) {
        bake_anim = scene->anim;
    }

    if (bake_anim) {
        ufbx_bake_opts opts;
        std::memset(&opts, 0, sizeof(opts));

        ufbx_error bake_error;
        std::memset(&bake_error, 0, sizeof(bake_error));

        ufbx_baked_anim* baked = ufbx_bake_anim(scene, bake_anim, &opts, &bake_error);
        if (baked) {
            baked_anim_clip_ = std::make_unique<BakedAnimClip>();
            baked_anim_clip_->start_time = (float)baked->playback_time_begin;
            baked_anim_clip_->end_time = (float)baked->playback_time_end;
            baked_anim_clip_->duration = (float)baked->playback_duration;
            baked_anim_clip_->sample_rate = 0.0f;

            baked_anim_clip_->channels.clear();
            baked_anim_clip_->channels.reserve(baked->nodes.count);

            for (size_t ni = 0; ni < baked->nodes.count; ++ni)
            {
                const ufbx_baked_node& bn = baked->nodes.data[ni];

                BakedAnimChannel ch;
                ch.node_index = bn.typed_id;

                ch.translation_keys.reserve(bn.translation_keys.count);
                for (size_t ki = 0; ki < bn.translation_keys.count; ++ki) {
                    const ufbx_baked_vec3& k = bn.translation_keys.data[ki];
                    BakedVec3Key dk;
                    dk.time = (float)k.time;
                    dk.v[0] = (float)k.value.x;
                    dk.v[1] = (float)k.value.y;
                    dk.v[2] = (float)k.value.z;
                    ch.translation_keys.push_back(dk);
                }

                ch.rotation_keys.reserve(bn.rotation_keys.count);
                for (size_t ki = 0; ki < bn.rotation_keys.count; ++ki) {
                    const ufbx_baked_quat& k = bn.rotation_keys.data[ki];
                    BakedQuatKey dk;
                    dk.time = (float)k.time;
                    dk.q[0] = (float)k.value.x;
                    dk.q[1] = (float)k.value.y;
                    dk.q[2] = (float)k.value.z;
                    dk.q[3] = (float)k.value.w;
                    ch.rotation_keys.push_back(dk);
                }

                ch.scale_keys.reserve(bn.scale_keys.count);
                for (size_t ki = 0; ki < bn.scale_keys.count; ++ki) {
                    const ufbx_baked_vec3& k = bn.scale_keys.data[ki];
                    BakedVec3Key dk;
                    dk.time = (float)k.time;
                    dk.v[0] = (float)k.value.x;
                    dk.v[1] = (float)k.value.y;
                    dk.v[2] = (float)k.value.z;
                    ch.scale_keys.push_back(dk);
                }

                baked_anim_clip_->channels.push_back(ch);
            }

            ufbx_free_baked_anim(baked);
        }
    }



    // Count mesh nodes
    size_t mesh_node_count = 0;
    for (size_t i = 0; i < scene->nodes.count; ++i)
    {
        const ufbx_node* node = scene->nodes.data[i];
        if (!node) continue;
        if (!node->mesh) continue;

        const ufbx_mesh* m = node->mesh;
        if (m->num_faces == 0) continue;

        ++mesh_node_count;
    }

    // Multi-mesh -> mesh_group_
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

            BakedMeshImportResult baked;
            if (!BakedMeshImporter::ImportMesh(scene, m, baked)) {
                return false;
            }

            std::unique_ptr<FbxMesh> sub = std::make_unique<FbxMesh>();
            if (!sub->BuildFromBaked(baked, scene, fbx_path)) {
                return false;
            }

            mesh_group_.AddMesh(std::move(sub));
        }

        if (mesh_group_.Empty()) {
            return false;
        }
    }
    else
    {
        // Single-mesh -> mesh_
        const ufbx_mesh* first_mesh = nullptr;
        for (size_t i = 0; i < scene->nodes.count; ++i)
        {
            const ufbx_node* node = scene->nodes.data[i];
            if (!node) continue;
            if (!node->mesh) continue;

            const ufbx_mesh* m = node->mesh;
            if (m->num_faces == 0) continue;

            first_mesh = m;
            break;
        }

        if (!first_mesh) {
            return false;
        }

        BakedMeshImportResult baked;
        if (!BakedMeshImporter::ImportMesh(scene, first_mesh, baked)) {
            return false;
        }

        if (!mesh_.BuildFromBaked(baked, scene, fbx_path)) {
            return false;
        }
    }

    // Initial pose
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

    // Step2: 同一 anim + 同一 timeSec なら skeleton 更新をスキップ
    if (anim == last_anim_ && t_sec == last_time_sec_)
    {
        pose_dirty_ = false;
        return;
    }

    last_anim_ = anim;
    last_time_sec_ = t_sec;
    pose_dirty_ = true;

    skeleton_.UpdateAtTime(scene, anim, t_sec);
}


//============================================================
// UpdateSkeletonAtTimeiAjj
//============================================================
void FbxModel::UpdateSkeletonAtTime(const ufbx_anim* anim, double t_sec)
{
    const ufbx_scene* scene = scene_.get();

    // Step2: 同一 anim + 同一 timeSec なら skeleton 更新をスキップ
    if (anim == last_anim_ && t_sec == last_time_sec_)
    {
        pose_dirty_ = false;
        return;
    }

    last_anim_ = anim;
    last_time_sec_ = t_sec;
    pose_dirty_ = true;

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
