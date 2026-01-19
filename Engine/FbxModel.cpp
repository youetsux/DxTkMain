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


    // Units: normalize to meters so all measurements are in meters.
    opts.target_unit_meters = 1.0;

    // Space conversion policy:
    // - Bake unit scaling into geometry, keep node transforms stable.
    //   This matches the typical "import meshes as they appear" expectation and
    //   keeps our downstream code (mesh build + skeleton bind math) consistent.
    opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
    ufbx_scene* raw_scene = ufbx_load_file(fbx_path, &opts, &err);

    if (!raw_scene) {
        return false;
    }

    scene_.reset(raw_scene);
    return true;
}
//============================================================
// MeasureBoundsFromScene
//  scene を保持しない運用でも targetHeight 正規化に必要な実寸を参照できるように、
//  ロード直後（scene が生きている間）に geometry_to_world を使ってメッシュ頂点をサンプルし AABB を作る
//============================================================
void FbxModel::MeasureBoundsFromScene(const ufbx_scene* scene)
{
    measured_height_ = 0.0f;
    measured_max_extent_ = 0.0f;
    measured_bounds_valid_ = false;

    if (!scene || !scene->root_node) return;

    float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
    float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
    bool first = true;

    std::vector<const ufbx_node*> stack;
    stack.reserve(256);
    stack.push_back(scene->root_node);

    while (!stack.empty())
    {
        const ufbx_node* node = stack.back();
        stack.pop_back();
        if (!node) continue;

        const size_t cc = node->children.count;
        for (size_t i = 0; i < cc; ++i)
        {
            const ufbx_node* c = node->children.data[i];
            if (c) stack.push_back(c);
        }

        if (!node->mesh) continue;
        const ufbx_mesh* m = node->mesh;
        if (!m->vertex_position.exists) continue;

        const size_t vcount = m->vertex_position.values.count;
        if (vcount == 0) continue;

        const size_t MAX_SAMPLE = 20000;
        size_t step = 1;
        if (vcount > MAX_SAMPLE) step = vcount / MAX_SAMPLE;

        for (size_t vi = 0; vi < vcount; vi += step)
        {
            const ufbx_vec3 p = m->vertex_position.values.data[vi];
            const ufbx_vec3 wp = ufbx_transform_position(&node->geometry_to_world, p);

            const float x = (float)wp.x;
            const float y = (float)wp.y;
            const float z = (float)wp.z;

            if (first)
            {
                minX = maxX = x;
                minY = maxY = y;
                minZ = maxZ = z;
                first = false;
            }
            else
            {
                if (x < minX) minX = x; if (x > maxX) maxX = x;
                if (y < minY) minY = y; if (y > maxY) maxY = y;
                if (z < minZ) minZ = z; if (z > maxZ) maxZ = z;
            }
        }
    }

    if (first) return;

    const float sx = (maxX - minX);
    const float sy = (maxY - minY);
    const float sz = (maxZ - minZ);

    // 最大軸長は常に保持（scene なし時の保険）
    float maxe = sx;
    if (sy > maxe) maxe = sy;
    if (sz > maxe) maxe = sz;
    measured_max_extent_ = maxe;

    // target_axes = left_handed_y_up でロードしているため、
 // geometry_to_world の結果は常に Y-up 空間。
 // scene->settings.axes は「元FBXの軸」なので使わない。
    measured_height_ = sy;
    measured_bounds_valid_ = true;
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


    // scene 破棄後でも targetHeight 正規化が安定するように、ここで実寸を計測
    MeasureBoundsFromScene(scene);

    // ------------------------------------------------------------
    // 令和大改革: アニメをロード時にベイクして保持し、ufbx_scene を破棄する
    //   - ランタイムは baked_clips_ から再生する（scene/ufbx_anim を保持しない）
    // ------------------------------------------------------------
    baked_clips_.clear();
    active_clip_index_ = 0;
    {
        const size_t bone_count = skeleton_.Bones().size();
        if (scene && bone_count > 0)
        {
            double fps = scene->settings.frames_per_second;
            if (fps <= 0.0) fps = 30.0;

            auto bake_one = [&](const ufbx_anim* anim, const char* name_cstr)
                {
                    if (!anim) return;
                    const double duration = anim->time_end - anim->time_begin;
                    if (duration <= 0.0) return;

                    int endFrame = (int)(duration * fps + 0.5);
                    if (endFrame < 1) endFrame = 1;
                    const int frame_count = endFrame + 1;

                    BakedClip clip;
                    clip.name = name_cstr ? name_cstr : std::string();
                    clip.fps = fps;
                    clip.frame_count = frame_count;
                    clip.world_frames.resize((size_t)frame_count * bone_count);

                    const double secondsPerFrame = 1.0 / fps;
                    for (int f = 0; f < frame_count; ++f)
                    {
                        const double t = anim->time_begin + (double)f * secondsPerFrame;
                        skeleton_.UpdateAtTime(scene, anim, t);
                        const auto& cw = skeleton_.CurrWorld();
                        const size_t base = (size_t)f * bone_count;
                        for (size_t i = 0; i < bone_count; ++i)
                        {
                            clip.world_frames[base + i] = cw[i];
                        }
                    }

                    baked_clips_.push_back(std::move(clip));
                };

            if (scene->anim_stacks.count > 0)
            {
                for (size_t i = 0; i < scene->anim_stacks.count; ++i)
                {
                    const ufbx_anim_stack* st = scene->anim_stacks.data[i];
                    if (!st || !st->anim) continue;
                    std::string nm;
                    if (st->name.data && st->name.length > 0) {
                        nm.assign(st->name.data, st->name.length);
                    }
                    bake_one(st->anim, nm.c_str());
                }
            }
            else
            {
                // anim_stacks が無い FBX でも default anim があればベイク
                bake_one(scene->anim, "Default");
            }
        }
    }

    // scene 破棄（以降ランタイムでは保持しない）
    skeleton_.DetachFromScene();
    scene_.reset(nullptr);
    last_anim_ = nullptr;
    last_time_sec_ = -1.0;
    pose_dirty_ = true;

    // 初期姿勢を適用
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
// scene 破棄運用（ベイク済み）: AnimStack 互換 API
//============================================================
int FbxModel::GetRuntimeAnimStackCount() const
{
    const ufbx_scene* scene = scene_.get();
    if (scene) {
        return (int)scene->anim_stacks.count;
    }
    return (int)baked_clips_.size();
}

std::string FbxModel::GetRuntimeAnimStackName(int index) const
{
    const ufbx_scene* scene = scene_.get();
    if (scene)
    {
        if (index < 0 || (size_t)index >= scene->anim_stacks.count) return std::string();
        const ufbx_anim_stack* st = scene->anim_stacks.data[index];
        if (!st) return std::string();
        if (st->name.data && st->name.length > 0) return std::string(st->name.data, st->name.length);
        return std::string();
    }
    if (index < 0 || (size_t)index >= baked_clips_.size()) return std::string();
    return baked_clips_[(size_t)index].name;
}

void FbxModel::SetRuntimeAnimStack(int index)
{
    const int count = GetRuntimeAnimStackCount();
    if (count <= 0) return;
    if (index < 0) index = 0;
    if (index >= count) index = count - 1;
    active_clip_index_ = index;
}

int FbxModel::GetRuntimeAnimStartFrame(int index) const
{
    (void)index;
    return 0;
}

int FbxModel::GetRuntimeAnimEndFrame(int index) const
{
    const ufbx_scene* scene = scene_.get();
    if (scene)
    {
        if (index < 0 || (size_t)index >= scene->anim_stacks.count) return 0;
        const ufbx_anim_stack* st = scene->anim_stacks.data[index];
        if (!st || !st->anim) return 0;
        double fps = scene->settings.frames_per_second;
        if (fps <= 0.0) fps = 30.0;
        const double duration = st->anim->time_end - st->anim->time_begin;
        int endFrame = (int)(duration * fps + 0.5);
        if (endFrame < 0) endFrame = 0;
        return endFrame;
    }
    if (index < 0 || (size_t)index >= baked_clips_.size()) return 0;
    const int fc = baked_clips_[(size_t)index].frame_count;
    return fc > 0 ? (fc - 1) : 0;
}

double FbxModel::GetRuntimeAnimFps(int index) const
{
    const ufbx_scene* scene = scene_.get();
    if (scene)
    {
        double fps = scene->settings.frames_per_second;
        if (fps <= 0.0) fps = 30.0;
        return fps;
    }
    if (index < 0 || (size_t)index >= baked_clips_.size()) return 30.0;
    return baked_clips_[(size_t)index].fps;
}

void FbxModel::UpdateSkeletonAtFrame(int stackIndex, double frame)
{
    const ufbx_scene* scene = scene_.get();
    if (scene)
    {
        // 旧経路: 秒で評価
        if (stackIndex >= 0 && (size_t)stackIndex < scene->anim_stacks.count)
        {
            const ufbx_anim_stack* st = scene->anim_stacks.data[stackIndex];
            if (st && st->anim)
            {
                const double fps = GetRuntimeAnimFps(stackIndex);
                const double secondsPerFrame = 1.0 / fps;
                const double t = st->anim->time_begin + frame * secondsPerFrame;
                UpdateSkeletonAtTime(st->anim, t);
                return;
            }
        }
        UpdateSkeletonAtTime(0.0);
        return;
    }

    if (baked_clips_.empty()) return;
    if (stackIndex < 0) stackIndex = 0;
    if ((size_t)stackIndex >= baked_clips_.size()) stackIndex = (int)baked_clips_.size() - 1;

    const BakedClip& clip = baked_clips_[(size_t)stackIndex];
    const size_t bone_count = skeleton_.Bones().size();
    if (bone_count == 0) return;
    if (clip.frame_count <= 0) return;

    int f = (int)std::floor(frame + 1e-6);
    if (f < 0) f = 0;
    if (f >= clip.frame_count) f = clip.frame_count - 1;

    const size_t base = (size_t)f * bone_count;
    skeleton_.ApplyBakedPoseWorld(&clip.world_frames[base], bone_count);
}

//============================================================
// UpdateSkeletonAtTimeiftHgAjj
//============================================================
void FbxModel::UpdateSkeletonAtTime(double t_sec)
{
    const ufbx_scene* scene = scene_.get();
    const ufbx_anim* anim = GetDefaultAnim();

    // scene を保持しない運用（ベイク済み）: t_sec をフレーム番号として解釈し、active_clip_index_ を再生
    if (!scene)
    {
        UpdateSkeletonAtFrame(active_clip_index_, t_sec);
        return;
    }

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

    // scene を保持しない運用（ベイク済み）: anim は使用せず、active_clip_index_ を再生
    if (!scene)
    {
        UpdateSkeletonAtFrame(active_clip_index_, t_sec);
        return;
    }

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
