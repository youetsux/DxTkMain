#define NOMINMAX
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

float FbxModel::SceneRadius()
{
    // まずスケルトンが持っているシーン半径を優先して返す
    float r = skeleton_.SceneRadius();   // FbxSkeleton::SceneRadius()

    if (r > 0.0f) {
        return r;
    }

    // スケルトンに有効な値がない場合は、メッシュの BV から半径を取る
    const BVolume& bv = mesh_.GetBV();
    if (bv.radius > 0.0f) {
        return bv.radius;
    }

    // どちらもダメならとりあえず 1.0 を返す（ゼロ除算防止用）
    return 1.0f;
}

float FbxModel::SceneHeight()
{
    return MeasureSize(SizeMeasureAxis::HeightY);
}

float FbxModel::MeasureSize(SizeMeasureAxis axis)
{
    using namespace DirectX;

    const auto& meshData = mesh_.Data();
    if (meshData.vertices_.empty()) {
        return 1.0f;   // メッシュ無し保険
    }

    // ボーンウェイトが無い → これまで通りメッシュの BV だけで測る
    if (meshData.influences_.empty()) {
        const BVolume& bv = mesh_.GetBV();

        float lenX = bv.max.x - bv.min.x;
        float lenY = bv.max.y - bv.min.y;
        float lenZ = bv.max.z - bv.min.z;

        auto safe = [](float v) { return (v > 0.0f) ? v : 1.0f; };

        switch (axis)
        {
        case SizeMeasureAxis::HeightY: return safe(lenY);
        case SizeMeasureAxis::WidthX:  return safe(lenX);
        case SizeMeasureAxis::DepthZ:  return safe(lenZ);
        case SizeMeasureAxis::MaxExtent:
        {
            float m = lenX;
            if (lenY > m) m = lenY;
            if (lenZ > m) m = lenZ;
            return safe(m);
        }
        case SizeMeasureAxis::Radius:
        default:
            return SceneRadius(); // 互換
        }
    }

    // -------------------------------
    // ここから「スキニング後の高さ」で測る
    // -------------------------------

    // 1) どのポーズで測るか：デフォルトアニメの先頭（なければ t=0）
    const ufbx_anim* anim = GetDefaultAnim();
    if (anim) {
        UpdateSkeletonAtTime(anim, anim->time_begin);
    }
    else {
        UpdateSkeletonAtTime(0.0);
    }

    // 2) スキン行列を組み立てる（Draw() と同じロジック）
    auto& bones = skeleton_.Bones();
    auto& currWorld = skeleton_.CurrWorld();
    auto& skinMats = skeleton_.SkinMatrices();

    size_t boneCount = bones.size();
    skinMats.resize(boneCount);

    for (size_t i = 0; i < boneCount; ++i) {
        XMMATRIX W = XMLoadFloat4x4(&currWorld[i]);
        XMMATRIX G2B = XMLoadFloat4x4(&bones[i].geom_bind_world);
        skinMats[i] = XMMatrixMultiply(G2B, W);
    }

    // 3) バインド頂点にスキンを掛けた位置で AABB を取る
    const auto& influences = meshData.influences_;
    const auto& bindVertices =
        meshData.bind_vertices_.empty()
        ? meshData.vertices_
        : meshData.bind_vertices_;

    XMFLOAT3 bbMin = { FLT_MAX,  FLT_MAX,  FLT_MAX };
    XMFLOAT3 bbMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    const size_t vcount = bindVertices.size();

    for (size_t v = 0; v < vcount; ++v) {
        const auto& src = bindVertices[v];

        XMVECTOR Pbind = XMLoadFloat3(&src.pos);
        XMVECTOR P = XMVectorZero();
        bool any = false;

        const auto& inf = influences[v];

        for (int k = 0; k < 4; ++k) {
            float    w = inf.weight[k];
            uint16_t b = inf.bone[k];

            if (w <= 0.0f)             continue;
            if (b >= skinMats.size())  continue;

            XMMATRIX B = skinMats[b];
            XMVECTOR W = XMVectorReplicate(w);

            XMVECTOR p = XMVector3Transform(Pbind, B);
            P = XMVectorMultiplyAdd(p, W, P);
            any = true;
        }

        if (!any) {
            P = Pbind; // ウェイト無しならバインド位置そのまま
        }

        XMFLOAT3 p;
        XMStoreFloat3(&p, P);

        if (p.x < bbMin.x) bbMin.x = p.x;
        if (p.y < bbMin.y) bbMin.y = p.y;
        if (p.z < bbMin.z) bbMin.z = p.z;

        if (p.x > bbMax.x) bbMax.x = p.x;
        if (p.y > bbMax.y) bbMax.y = p.y;
        if (p.z > bbMax.z) bbMax.z = p.z;
    }

    float lenX = bbMax.x - bbMin.x;
    float lenY = bbMax.y - bbMin.y;
    float lenZ = bbMax.z - bbMin.z;

    auto safe = [](float v) { return (v > 0.0f) ? v : 1.0f; };

    switch (axis)
    {
    case SizeMeasureAxis::HeightY:
        return safe(lenY);
    case SizeMeasureAxis::WidthX:
        return safe(lenX);
    case SizeMeasureAxis::DepthZ:
        return safe(lenZ);
    case SizeMeasureAxis::MaxExtent:
    {
        float m = lenX;
        if (lenY > m) m = lenY;
        if (lenZ > m) m = lenZ;
        return safe(m);
    }
    case SizeMeasureAxis::Radius:
    default:
        return SceneRadius(); // ここは互換用（使わないなら消してもよい）
    }
}

float FbxModel::MeasureSkinnedHeightY()
{
    const ufbx_scene* scene = scene_.get();
    const ufbx_anim* anim = GetDefaultAnim();

    // アニメが無い or シーンが無い場合 → メッシュ高さで代用
    if (!scene || !anim) {
        return MeasureSize(SizeMeasureAxis::HeightY);
    }

    // 1) デフォルトポーズにスケルトンを合わせる
    double t0 = anim->time_begin;
    UpdateSkeletonAtTime(anim, t0); // ★ ここで curr_world_ が更新される

    // 2) Draw() と同じ手順で skin_mats を作って CPU スキニング実行
    const auto& bones = skeleton_.Bones();
    const auto& curr = skeleton_.CurrWorld();

    std::vector<DirectX::XMMATRIX> skin_mats;
    skin_mats.resize(bones.size());

    for (size_t i = 0; i < bones.size(); ++i) {
        DirectX::XMMATRIX bind = DirectX::XMLoadFloat4x4(&bones[i].geom_bind_world);
        DirectX::XMMATRIX cw = DirectX::XMLoadFloat4x4(&curr[i]);
        skin_mats[i] = bind * cw;
    }

    // ★ここで skinned_vertices_ が埋まる
    mesh_.ApplySkinCPU(skin_mats);

    // 3) skinned_vertices_ から Y の min/max を取る
    const auto& sv = mesh_.Data().skinned_vertices_;
    if (sv.empty()) {
        // 念のための保険
        return MeasureSize(SizeMeasureAxis::HeightY);
    }

    float minY = FLT_MAX;
    float maxY = -FLT_MAX;
    for (const auto& v : sv) {
        if (v.pos.y < minY) minY = v.pos.y;
        if (v.pos.y > maxY) maxY = v.pos.y;
    }

    if (minY > maxY) {
        // ここにはもう基本落ちてこない想定だけど一応
        return MeasureSize(SizeMeasureAxis::HeightY);
    }

    return maxY - minY;
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
    UpdateSkeletonAtTime(0.0);
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
