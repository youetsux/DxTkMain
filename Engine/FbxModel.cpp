#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "FbxModel.h"
#include "BakedRig.h"
#include "BakedAnim.h"
#include "BakedMeshImporter.h"
#include "BakedDataValidate.h"
#include "BakedPoseEval.h"
#include "UfbxUtil.h"

#include <cstring>
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <new> // placement new
#include <unordered_map>
#include <sstream>

#ifndef BAKED_IMPORT_VALIDATE_TRANSFORMS
#define BAKED_IMPORT_VALIDATE_TRANSFORMS 0
#endif

#ifndef BAKED_FILTER_STATIC_CHANNELS
// 1: rest(local) と同一の静止ノードを channels から除外する
// 0: すべてのノードを channels に保持する（デバッグ優先）
#define BAKED_FILTER_STATIC_CHANNELS 0
#endif

#ifndef BAKED_BAKE_DEBUG
// 1: ufbx_bake_anim() の結果（チャンネル数/キー数）を OutputDebugStringA に出す
// 0: 出さない
#define BAKED_BAKE_DEBUG 0
#endif

#ifndef BAKED_RUNTIME_DEBUG
// 1: baked 再生時に「評価結果が開始姿勢と同一か」を簡易チェックしてログ出しする
// 0: 出さない
#define BAKED_RUNTIME_DEBUG 0
#endif

namespace
{
    // ufbx_bake_anim() のリサンプルレート（ゲーム用途の基準）
    static const double kBakeResampleRateFps = 60.0;

    static std::unique_ptr<BakedAnimClip> BakeAnimToClip(const ufbx_scene* scene, const ufbx_anim* anim, double resample_rate_fps, std::string& out_error)
    {
        if (!scene || !anim) return nullptr;

        ufbx_bake_opts opts;
        std::memset(&opts, 0, sizeof(opts));
        opts.resample_rate = resample_rate_fps;

        ufbx_error bake_error;
        std::memset(&bake_error, 0, sizeof(bake_error));

        ufbx_baked_anim* baked = ufbx_bake_anim(scene, anim, &opts, &bake_error);
        if (!baked)
        {
            out_error.clear();
            if (bake_error.description.data && bake_error.description.length > 0)
            {
                out_error.append(bake_error.description.data, bake_error.description.length);
            }
            else
            {
                out_error = "ufbx_bake_anim failed";
            }
            if (bake_error.info_length > 0)
            {
                out_error += ": ";
                out_error.append(bake_error.info, bake_error.info_length);
            }
            return nullptr;
        }

        std::unique_ptr<BakedAnimClip> clip = std::make_unique<BakedAnimClip>();
        clip->start_time = (float)baked->playback_time_begin;
        clip->end_time = (float)baked->playback_time_end;
        clip->duration = (float)baked->playback_duration;
        clip->sample_rate = (float)opts.resample_rate;

        clip->channels.clear();
        clip->channels.reserve(baked->nodes.count);

        for (size_t ni = 0; ni < baked->nodes.count; ++ni)
        {
            const ufbx_baked_node& bn = baked->nodes.data[ni];

            BakedAnimChannel ch;
            ch.node_index = bn.typed_id;

            ch.translation_keys.reserve(bn.translation_keys.count);
            for (size_t ki = 0; ki < bn.translation_keys.count; ++ki)
            {
                const ufbx_baked_vec3& k = bn.translation_keys.data[ki];
                BakedVec3Key dk;
                dk.time = (float)k.time;
                dk.v[0] = (float)k.value.x;
                dk.v[1] = (float)k.value.y;
                dk.v[2] = (float)k.value.z;
                ch.translation_keys.push_back(dk);
            }

            ch.rotation_keys.reserve(bn.rotation_keys.count);
            for (size_t ki = 0; ki < bn.rotation_keys.count; ++ki)
            {
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
            for (size_t ki = 0; ki < bn.scale_keys.count; ++ki)
            {
                const ufbx_baked_vec3& k = bn.scale_keys.data[ki];
                BakedVec3Key dk;
                dk.time = (float)k.time;
                dk.v[0] = (float)k.value.x;
                dk.v[1] = (float)k.value.y;
                dk.v[2] = (float)k.value.z;
                ch.scale_keys.push_back(dk);
            }

            clip->channels.push_back(std::move(ch));
        }

        ufbx_free_baked_anim(baked);
        return clip;
    }
}


//------------------------------------------------------------
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
    scene_.reset(nullptr);

    baked_rig_.reset();
    baked_anim_stacks_.clear();
    baked_active_stack_index_ = -1;
    baked_default_clip_.reset();
    baked_up_axis_ = 0;
    last_error_.clear();

    skeleton_.~FbxSkeleton();
    new (&skeleton_) FbxSkeleton();

    mesh_.~FbxMesh();
    new (&mesh_) FbxMesh();

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
        last_error_.clear();
        if (err.description.data && err.description.length > 0) {
            last_error_.append(err.description.data, err.description.length);
        }
        else {
            last_error_ = "ufbx_load_file failed";
        }
        if (err.info_length > 0) {
            last_error_ += ": ";
            last_error_.append(err.info, err.info_length);
        }
        return false;
    }

    scene_.reset(raw_scene);
    // discard 後でも Model 側で参照できるよう保存
    baked_up_axis_ = (int)raw_scene->settings.axes.up;
    return true;
}

const std::string& FbxModel::GetLastError() const
{
    return last_error_;
}

//============================================================
// Baked clip/anim stacks API (works even after DiscardScene())
//============================================================
const BakedAnimClip* FbxModel::GetBakedAnimClip() const
{
    if (baked_active_stack_index_ >= 0 && (size_t)baked_active_stack_index_ < baked_anim_stacks_.size())
    {
        const BakedAnimStack& st = baked_anim_stacks_[(size_t)baked_active_stack_index_];
        return st.clip.get();
    }
    return baked_default_clip_.get();
}

int FbxModel::GetAnimStackCount() const
{
    if (!baked_anim_stacks_.empty())
    {
        return (int)baked_anim_stacks_.size();
    }

    const ufbx_scene* scene = scene_.get();
    if (scene) return (int)scene->anim_stacks.count;
    return 0;
}

std::string FbxModel::GetAnimStackName(int index) const
{
    if (!baked_anim_stacks_.empty())
    {
        if (index < 0 || (size_t)index >= baked_anim_stacks_.size()) return {};
        return baked_anim_stacks_[(size_t)index].name;
    }

    const ufbx_scene* scene = scene_.get();
    if (!scene) return {};
    if (index < 0 || (size_t)index >= scene->anim_stacks.count) return {};
    const ufbx_anim_stack* st = scene->anim_stacks.data[index];
    if (!st) return {};
    return std::string(st->name.data, st->name.length);
}

bool FbxModel::SetAnimStack(int index)
{
    if (!baked_anim_stacks_.empty())
    {
        if (index < 0 || (size_t)index >= baked_anim_stacks_.size()) return false;
        baked_active_stack_index_ = index;
        return true;
    }

    // If scene is still alive, keep track of the selected index so that
    // GetBakedAnimClip() can follow it after LoadBaked() and DiscardScene().
    const ufbx_scene* scene = scene_.get();
    if (!scene) return false;
    if (index < 0 || (size_t)index >= scene->anim_stacks.count) return false;
    baked_active_stack_index_ = index;
    return true;
}

bool FbxModel::SetAnimStack(const std::string& stackName)
{
    if (stackName.empty()) return false;

    const int count = GetAnimStackCount();
    for (int i = 0; i < count; ++i)
    {
        if (GetAnimStackName(i) == stackName)
        {
            return SetAnimStack(i);
        }
    }
    return false;
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
    // ------------------------------------------------------------
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

    // ------------------------------------------------------------
    // ------------------------------------------------------------
    if (!mesh_.BuildFromScene(scene, skeleton_, fbx_path)) {
        return false;
    }

    // ------------------------------------------------------------
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

    // StepB-3: Bake ALL anim stacks so SetAnimStack() can work after DiscardScene().
    baked_anim_stacks_.clear();
    baked_active_stack_index_ = -1;
    baked_default_clip_.reset();

    if (scene->anim_stacks.count > 0)
    {
        baked_anim_stacks_.reserve(scene->anim_stacks.count);

        for (size_t si = 0; si < scene->anim_stacks.count; ++si)
        {
            const ufbx_anim_stack* st = scene->anim_stacks.data[si];
            if (!st) continue;

            std::string bake_err;
            std::unique_ptr<BakedAnimClip> clip = BakeAnimToClip(scene, st->anim, kBakeResampleRateFps, bake_err);
            if (!clip)
            {
                last_error_ = bake_err;
                return false;
            }

            BakedAnimStack dst;
            dst.name.assign(st->name.data, st->name.length);
            dst.clip = std::move(clip);
            baked_anim_stacks_.push_back(std::move(dst));
        }

        if (!baked_anim_stacks_.empty())
        {
            baked_active_stack_index_ = 0;
        }
    }
    else
    {
        // No anim stacks. Still bake scene->anim if present.
        if (scene->anim)
        {
            std::string bake_err;
            baked_default_clip_ = BakeAnimToClip(scene, scene->anim, kBakeResampleRateFps, bake_err);
            if (!baked_default_clip_)
            {
                last_error_ = bake_err;
                return false;
            }
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

    if (anim == last_anim_ && t_sec == last_time_sec_)
    {
        pose_dirty_ = false;
        return;
    }

    last_anim_ = anim;
    last_time_sec_ = t_sec;
    pose_dirty_ = true;

    if (scene && anim)
    {
        skeleton_.UpdateAtTime(scene, anim, t_sec);
        return;
    }

    // scene 破棄後は baked データから評価する
    if (baked_rig_)
    {
        std::vector<DirectX::XMMATRIX> node_world;
        const BakedAnimClip* clip = GetBakedAnimClip();
        BakedPoseEval::EvaluateNodeWorld(*baked_rig_, clip, (float)t_sec, node_world);
        skeleton_.UpdateFromBakedWorldMatrices(node_world);

#if BAKED_RUNTIME_DEBUG
        // time は進んでいるのに動かないケースの切り分け：
        // ・clip が無い/キーが無い/参照できていない場合、任意ノードの world が開始時と同一になりやすい。
        static int s_dbgCount = 0;
        if ((s_dbgCount++ % 120) == 0) // だいたい2秒に1回
        {
            if (clip)
            {
                std::vector<DirectX::XMMATRIX> node_world0;
                BakedPoseEval::EvaluateNodeWorld(*baked_rig_, clip, clip->start_time, node_world0);

                if (!node_world.empty() && node_world0.size() == node_world.size())
                {
                    DirectX::XMFLOAT4X4 a, b;
                    DirectX::XMStoreFloat4x4(&a, node_world[0]);
                    DirectX::XMStoreFloat4x4(&b, node_world0[0]);
                    const float dx = std::fabs(a._41 - b._41);
                    const float dy = std::fabs(a._42 - b._42);
                    const float dz = std::fabs(a._43 - b._43);

                    char buf[256];
                    sprintf_s(buf, "[BakedRuntime] t=%.6f start=%.6f node0_dpos=(%.6g,%.6g,%.6g) channels=%zu\n",
                        t_sec,
                        (double)clip->start_time,
                        (double)dx, (double)dy, (double)dz,
                        clip->channels.size());
                    OutputDebugStringA(buf);
                }
                else
                {
                    char buf[256];
                    sprintf_s(buf, "[BakedRuntime] t=%.6f start=%.6f node_world=%zu node_world0=%zu channels=%zu\n",
                        t_sec,
                        (double)clip->start_time,
                        node_world.size(),
                        node_world0.size(),
                        clip->channels.size());
                    OutputDebugStringA(buf);
                }
            }
            else
            {
                OutputDebugStringA("[BakedRuntime] clip=null\n");
            }
        }
#endif
    }
}


//============================================================
// UpdateSkeletonAtTimeiAjj
//============================================================
void FbxModel::UpdateSkeletonAtTime(const ufbx_anim* anim, double t_sec)
{
    const ufbx_scene* scene = scene_.get();

    if (anim == last_anim_ && t_sec == last_time_sec_)
    {
        pose_dirty_ = false;
        return;
    }

    last_anim_ = anim;
    last_time_sec_ = t_sec;
    pose_dirty_ = true;

    if (scene && anim)
    {
        skeleton_.UpdateAtTime(scene, anim, t_sec);
        return;
    }

    // scene 破棄後は baked データから評価する
    if (baked_rig_)
    {
        std::vector<DirectX::XMMATRIX> node_world;
        const BakedAnimClip* clip = GetBakedAnimClip();
        BakedPoseEval::EvaluateNodeWorld(*baked_rig_, clip, (float)t_sec, node_world);
        skeleton_.UpdateFromBakedWorldMatrices(node_world);
    }
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

bool FbxModel::ValidateBakedData(std::string& out_error) const
{
    out_error.clear();
    if (!baked_rig_)
    {
        out_error = "baked_rig_: null";
        return false;
    }

    if (!baked_anim_stacks_.empty())
    {
        for (size_t i = 0; i < baked_anim_stacks_.size(); ++i)
        {
            const BakedAnimClip* clip = baked_anim_stacks_[i].clip.get();
            if (!BakedDataValidate::ValidateRigAndClip(*baked_rig_, clip, out_error))
                return false;
        }
    }
    else
    {
        const BakedAnimClip* clip = baked_default_clip_.get();
        if (!BakedDataValidate::ValidateRigAndClip(*baked_rig_, clip, out_error))
            return false;
    }

#if BAKED_IMPORT_VALIDATE_TRANSFORMS
    if (!ValidateBakedTransforms(out_error))
        return false;
#endif

    return true;
}

static void SetValidateErr(std::string& out_error, const std::string& s)
{
    out_error = s;
}

bool FbxModel::ValidateBakedTransforms(std::string& out_error) const
{
    out_error.clear();

    const ufbx_scene* scene = scene_.get();
    if (!scene)
    {
        out_error = "ValidateBakedTransforms: scene_: null (call before DiscardScene)";
        return false;
    }
    if (!baked_rig_)
    {
        out_error = "ValidateBakedTransforms: baked_rig_: null";
        return false;
    }

    const BakedRig& rig = *baked_rig_;
    const BakedAnimClip* clip = GetBakedAnimClip();

    const ufbx_anim* anim = nullptr;
    if (scene->anim_stacks.count > 0)
    {
        size_t si = 0;
        if (baked_active_stack_index_ >= 0 && (size_t)baked_active_stack_index_ < scene->anim_stacks.count)
        {
            si = (size_t)baked_active_stack_index_;
        }
        const ufbx_anim_stack* st = scene->anim_stacks.data[si];
        if (st) anim = st->anim;
    }
    if (!anim) anim = scene->anim;

    std::vector<float> times;
    if (clip && clip->duration > 0.0f)
    {
        const float t0 = clip->start_time;
        const float t1 = (clip->start_time + clip->end_time) * 0.5f;
        const float t2 = clip->end_time - 1.0e-4f;
        times.push_back(t0);
        times.push_back(t1);
        times.push_back(t2);
    }
    else
    {
        times.push_back(0.0f);
    }

    for (float t : times)
    {
        std::vector<DirectX::XMMATRIX> baked_world;
        BakedPoseEval::EvaluateNodeWorld(rig, clip, t, baked_world);

        if (baked_world.size() != rig.nodes.size())
        {
            std::ostringstream oss;
            oss << "ValidateBakedTransforms: baked_world.size mismatch "
                << baked_world.size() << " vs rig.nodes " << rig.nodes.size();
            SetValidateErr(out_error, oss.str());
            return false;
        }

        std::unordered_map<const ufbx_node*, DirectX::XMFLOAT4X4> cache;

        for (size_t i = 0; i < scene->nodes.count; ++i)
        {
            const ufbx_node* node = scene->nodes.data[i];
            if (!node) continue;

            const uint32_t node_index = node->typed_id;
            if (node_index >= baked_world.size()) continue;

            DirectX::XMFLOAT4X4 ufbxW = UfbxUtil::EvaluateNodeWorldRecursive(node, anim, (double)t, cache);

            DirectX::XMFLOAT4X4 bakedW;
            DirectX::XMStoreFloat4x4(&bakedW, baked_world[node_index]);

            const float eps = 1.0e-3f;
            const float* a = &ufbxW.m[0][0];
            const float* b = &bakedW.m[0][0];
            for (int k = 0; k < 16; ++k)
            {
                const float da = a[k] - b[k];
                const float ad = da < 0.0f ? -da : da;
                if (ad > eps)
                {
                    std::ostringstream oss;
                    oss << "ValidateBakedTransforms: mismatch time=" << t
                        << " node_typed_id=" << node_index
                        << " element_id=" << node->element_id
                        << " k=" << k
                        << " ufbx=" << a[k]
                        << " baked=" << b[k]
                        << " diff=" << ad;
                    SetValidateErr(out_error, oss.str());
                    return false;
                }
            }
        }
    }

    return true;
}

void FbxModel::DiscardScene()
{
    scene_.reset();
}

bool FbxModel::ValidateAndDiscard(std::string& out_error)
{
    if (!ValidateBakedData(out_error))
        return false;

    DiscardScene();
    return true;
}

bool FbxModel::LoadBakedAndDiscard(const char* fbx_path, std::string& out_error)
{
    return LoadBakedAndDiscardEx(fbx_path, out_error, false);
}

bool FbxModel::LoadBakedAndDiscardEx(const char* fbx_path, std::string& out_error, bool validate_transforms)
{
    out_error.clear();
    if (!LoadBaked(fbx_path)) {
        if (!last_error_.empty()) {
            out_error = last_error_;
        }
        else {
            out_error = "LoadBaked failed";
        }
        return false;
    }

    if (!ValidateBakedData(out_error))
        return false;

    if (validate_transforms) {
        if (!ValidateBakedTransforms(out_error))
            return false;
    }

    DiscardScene();
    return true;
}
