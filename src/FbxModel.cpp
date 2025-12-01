#include "FbxModel.h"

#include <Windows.h>
#include <cstring>

#include "ufbx.h"

using namespace DirectX;

//============================================================
// コンストラクタ / デストラクタ
//============================================================
FbxModel::FbxModel()
    : scene_(nullptr, ufbx_free_scene)
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
    // シーン解放
    scene_.reset(nullptr);

    // スケルトン／メッシュも初期化
    skeleton_ = FbxSkeleton{};
    mesh_ = FbxMesh{};
}

//============================================================
// LoadScene（内部）
//   - ufbx_load_file でシーンを読む
//   - DirectX 向けの座標系変換オプションは UfbxStaticModel と揃える
//============================================================
bool FbxModel::LoadScene(const char* fbx_path)
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

    scene_.reset(raw_scene);   // デリータはコンストラクタで ufbx_free_scene を指定済み
    return true;
}

//============================================================
// Load（公開）
//   - FBX 読み込み
//   - FbxSkeleton 構築
//   - FbxMesh 構築（頂点展開＋GPUバッファ＋テクスチャ）
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

    // スケルトン構築
    if (!skeleton_.BuildFromScene(scene)) {
        return false;
    }

    // メッシュ構築（頂点展開＋GPUバッファ＋テクスチャ）
    if (!mesh_.BuildFromScene(scene, skeleton_, fbx_path)) {
        return false;
    }

    return true;
}

//============================================================
// GetDefaultAnim
//  （UfbxStaticModel と同等に、まず scene->anim を見る）
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
// UpdateSkeletonAtTime（明示的 anim 指定）
//============================================================
void FbxModel::UpdateSkeletonAtTime(const ufbx_anim* anim, double t_sec)
{
    const ufbx_scene* scene = scene_.get();
    skeleton_.UpdateAtTime(scene, anim, t_sec);
}

//============================================================
// Draw（CPU スキニング＋描画）
//============================================================
void FbxModel::Draw(
    const DirectX::XMMATRIX& world,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& proj)
{
    // スケルトンは事前に UpdateSkeletonAtTime() 済みの前提
    mesh_.Draw(world, view, proj, skeleton_);
}

//============================================================
// DrawSkeleton（ボーンデバッグ描画）
//============================================================
void FbxModel::DrawSkeleton(
    const DirectX::XMMATRIX& world,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& proj)
{
    skeleton_.DrawDebug(world, view, proj);
}
