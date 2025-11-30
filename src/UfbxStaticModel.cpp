#include "UfbxStaticModel.h"

#include <Windows.h>
#include <filesystem>
#include <string>
#include <cstring>

#include "ufbx.h"
#include "UfbxUtil.h"

using namespace DirectX;
namespace fs = std::filesystem;

//================================================================
// UfbxStaticModel: コンストラクタ
//================================================================

UfbxStaticModel::UfbxStaticModel()
{
    // 特に何もしない（scene_ は LoadScene() 時に設定）
}

//================================================================
// シーン読み込みラッパ
//================================================================
bool UfbxStaticModel::LoadScene(
    const char* fbx_path,
    std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)>& scene_out)
{
    ufbx_error     err;
    ufbx_load_opts opts;

    std::memset(&err, 0, sizeof(err));
    std::memset(&opts, 0, sizeof(opts));

    // 左手系 / Y-up / Z-forward に変換（DirectX に合わせる）
    opts.target_axes = ufbx_axes_left_handed_y_up;
    opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;

    ufbx_scene* raw_scene = ufbx_load_file(fbx_path, &opts, &err);
    if (!raw_scene) {
        return false;
    }

    scene_out = std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)>(
        raw_scene, ufbx_free_scene);
    return true;
}

//================================================================
// スケルトン構築ラッパ
//================================================================
bool UfbxStaticModel::BuildSkeletonFromScene(const ufbx_scene* scene)
{
    // 中身は FbxSkeleton に委譲するだけ
    return skeleton_.BuildFromScene(scene);
}

//================================================================
// Load（公開 API）
//   - FBX 読み込み
//   - スケルトン構築
//   - メッシュ展開（UfbxStaticModel_Mesh.cpp 側の実装を呼ぶ）
//   - GPU バッファ作成
//   - テクスチャ・エフェクト作成
//================================================================
bool UfbxStaticModel::Load(const char* fbx_path)
{
    // シーン読み込み
    if (!LoadScene(fbx_path, scene_)) {
        return false;
    }

    const ufbx_scene* scene = scene_.get();

    // スケルトン構築（FbxSkeleton 側へ委譲）
    if (!BuildSkeletonFromScene(scene)) {
        return false;
    }

    // メッシュ展開 + GPU バッファ作成 + テクスチャ・エフェクト作成
    // （実装は FbxMesh に集約）
    if (!mesh_.BuildFromScene(scene, skeleton_, fbx_path)) {
        return false;
    }

    //// GPU バッファ作成
    //if (!CreateGpuBuffers()) {
    //    return false;
    //}

    //// テクスチャ・エフェクト作成
    //if (!CreateEffectsAndTextures(fbx_path, scene)) {
    //    return false;
    //}

    // ExpandAllNodes() 内で
    //   - skeleton_.Data().scene_radius_
    //   - mesh_.bind_vertices_
    //   - mesh_.skinned_vertices_
    // はセット済みの前提

    return true;
}

//================================================================
// デフォルトアニメ取得
//================================================================
const ufbx_anim* UfbxStaticModel::GetDefaultAnim() const
{
    return scene_ ? scene_.get()->anim : nullptr;
}

//================================================================
// スケルトン更新ラッパ（オーバーロード群）
//================================================================

// (1) 内部に持っている scene_ のデフォルトアニメを使う
void UfbxStaticModel::UpdateSkeletonAtTime(double t_sec)
{
    const ufbx_scene* scene = scene_.get();
    const ufbx_anim* anim = GetDefaultAnim();
    skeleton_.UpdateAtTime(scene, anim, t_sec);
}

// (2) scene / anim を直接指定する下請け（他のオーバーロードから使用）
void UfbxStaticModel::UpdateSkeletonAtTime(
    const ufbx_scene* scene,
    const ufbx_anim* anim,
    double            t_sec)
{
    skeleton_.UpdateAtTime(scene, anim, t_sec);
}

// (3) scene のデフォルト anima を使う
void UfbxStaticModel::UpdateSkeletonAtTime(
    const ufbx_scene* scene,
    double            t_sec)
{
    const ufbx_anim* anim = scene ? scene->anim : nullptr;
    UpdateSkeletonAtTime(scene, anim, t_sec);
}

// (4) アニメーションスタックを index で選ぶ
void UfbxStaticModel::UpdateSkeletonAtTime(
    const ufbx_scene* scene,
    size_t            stack_index,
    double            t_sec)
{
    const ufbx_anim* anim = nullptr;

    if (scene && stack_index < scene->anim_stacks.count) {
        const ufbx_anim_stack* st = scene->anim_stacks.data[stack_index];
        if (st) {
            anim = st->anim;
        }
    }

    UpdateSkeletonAtTime(scene, anim, t_sec);
}

// (5) アニメーションスタックを名前で選ぶ
void UfbxStaticModel::UpdateSkeletonAtTime(
    const ufbx_scene* scene,
    const std::string& stack_name,
    double                 t_sec)
{
    const ufbx_anim* anim = nullptr;

    if (scene && !stack_name.empty()) {
        for (size_t i = 0; i < scene->anim_stacks.count; ++i) {
            const ufbx_anim_stack* st = scene->anim_stacks.data[i];
            if (!st || !st->name.data) continue;

            if (std::strcmp(st->name.data, stack_name.c_str()) == 0) {
                anim = st->anim;
                break;
            }
        }
    }

    UpdateSkeletonAtTime(scene, anim, t_sec);
}

//================================================================
// メッシュ描画ラッパ
//================================================================
void UfbxStaticModel::Draw(
    const DirectX::XMMATRIX& world,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& proj)
{
    // FbxMesh に実装された Draw() に委譲する
    mesh_.Draw(world, view, proj, skeleton_);
}


//================================================================
// スケルトンデバッグ描画ラッパ
//================================================================
void UfbxStaticModel::DrawSkeleton(
    const DirectX::XMMATRIX& world,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& proj)
{
    skeleton_.DrawDebug(world, view, proj);
}

