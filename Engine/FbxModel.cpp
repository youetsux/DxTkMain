#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "FbxModel.h"

#include <cstring>
#include <cfloat>
#include <algorithm>
#include <new> // placement new

//------------------------------------------------------------
// 変更点（ビルド修正）
// - FbxMesh は unique_ptr を持つためコピー代入不可。
//   Reset() で「代入で初期化」せず、明示的に破棄→再構築する。
// - Step1: mesh_group_ を追加したので Reset() で Clear() する。
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

    // ※ コピー/ムーブ代入に頼らず「破棄→再構築」する
    //    （FbxMesh がコピー不可なため）
    skeleton_.~FbxSkeleton();
    new (&skeleton_) FbxSkeleton();

    mesh_.~FbxMesh();
    new (&mesh_) FbxMesh();

    // Step1: グループも残骸を残さない（まだ未使用）
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

    // スキニング結果頂点が有効ならそれを使う
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

    // まだスキニング結果が無い（またはスキン無し）なら AABB 高さ
    return MeasureSize(SizeMeasureAxis::HeightY);
}

//============================================================
// LoadScene（内部）
//============================================================
bool FbxModel::LoadScene(const char* fbx_path)
{
    ufbx_error     err;
    ufbx_load_opts opts;

    std::memset(&err, 0, sizeof(err));
    std::memset(&opts, 0, sizeof(opts));

    // DirectX 向け座標系
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

    // Skeleton 構築
    if (!skeleton_.BuildFromScene(scene)) {
        return false;
    }

    // Mesh 構築（現状は単一メッシュ経路）
    if (!mesh_.BuildFromScene(scene, skeleton_, fbx_path)) {
        return false;
    }

    // 初期姿勢
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
// UpdateSkeletonAtTime（明示アニメ）
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
    mesh_.Draw(world, view, proj, skeleton_);
}

//============================================================
// DrawSkeleton（デバッグ）
//============================================================
void FbxModel::DrawSkeleton(
    const DirectX::XMMATRIX& world,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& proj)
{
    skeleton_.DrawDebug(world, view, proj);
}
