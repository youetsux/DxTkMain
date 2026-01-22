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
    // scene の解放
    scene_.reset(nullptr);

    // ※ コピー/ムーブ禁止に関わらず「破棄→再構築」
    //   （FbxMesh はコピー不可なため）
    skeleton_.~FbxSkeleton();
    new (&skeleton_) FbxSkeleton();

    mesh_.~FbxMesh();
    new (&mesh_) FbxMesh();

    // マルチメッシュグループもクリア
    mesh_group_.Clear();
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
// UpdateSkeletonAtTime（デフォルトアニメ）
//============================================================
void FbxModel::UpdateSkeletonAtTime(double t_sec)
{
    const ufbx_scene* scene = scene_.get();
    const ufbx_anim* anim = GetDefaultAnim();
    skeleton_.UpdateAtTime(scene, anim, t_sec);
}


//============================================================
// UpdateSkeletonAtTime（アニメ指定）
//============================================================
void FbxModel::UpdateSkeletonAtTime(const ufbx_anim* anim, double t_sec)
{
    const ufbx_scene* scene = scene_.get();
    skeleton_.UpdateAtTime(scene, anim, t_sec);
}


//============================================================
// Draw（CPU スキニング＋描画）
//  マルチメッシュがあればグループ描画
//============================================================
void FbxModel::Draw(
    const DirectX::XMMATRIX& world,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& proj)
{
    if (!mesh_group_.Empty())
    {
        mesh_group_.Draw(world, view, proj, skeleton_);
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
