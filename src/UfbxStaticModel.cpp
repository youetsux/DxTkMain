#include "UfbxStaticModel.h"

#include <Windows.h>
#include <filesystem>
#include <unordered_map>
#include <string>
#include <cstring>

#include <WICTextureLoader.h>
#include "Gfx.h"
#include "ufbx.h"
#include "UfbxUtil.h"

using Microsoft::WRL::ComPtr;

namespace
{
    using namespace DirectX;
    namespace fs = std::filesystem;

    // ------------------------------------------------------------
    // デバッグ用頂点（ボーン表示のライン描画に使う）
    // ------------------------------------------------------------
    struct DebugVC
    {
        XMFLOAT3 pos; // 頂点位置
        XMFLOAT4 col; // 頂点カラー（RGBA）
    };

    // ------------------------------------------------------------
    // 骨の位置ヘルパ（DrawSkeleton で使う）
    // ------------------------------------------------------------
    DirectX::XMFLOAT3 GetBonePosition(const DirectX::XMFLOAT4X4& M)
    {
        // 行列の第4行の xyz が平行移動成分
        DirectX::XMFLOAT3 p;
        p.x = M._41;
        p.y = M._42;
        p.z = M._43;
        return p;
    }

    // ------------------------------------------------------------
    // ボーン登録ヘルパ
    // ・ufbx_node を SkeletonData に追加し、そのインデックスを返す
    // ・すでに登録されている場合は既存のインデックスを返す
    // ------------------------------------------------------------
    int AddBoneInternal(
        const ufbx_node* node,
        UfbxStaticModel::SkeletonData& skeleton,
        std::unordered_map<const ufbx_node*, int>& index_of)
    {
        // すでに登録済みかチェック
        std::unordered_map<const ufbx_node*, int>::iterator it =
            index_of.find(node);
        if (it != index_of.end()) {
            return it->second;
        }

        // 新しいボーンとして追加
        int idx = static_cast<int>(skeleton.bones_.size());
        index_of.insert(std::make_pair(node, idx));

        UfbxStaticModel::BoneInfo info;
        info.node = node;
        skeleton.bones_.push_back(info);
        return idx;
    }
} // anonymous namespace

//================================================================
// UfbxStaticModel: コンストラクタ／シーン読み込みラッパ
//================================================================

//----------------------------------------------------------------
// コンストラクタ
// ・メンバはヘッダ側のデフォルト初期化に任せており、ここでは何もしない
//----------------------------------------------------------------
UfbxStaticModel::UfbxStaticModel()
{
}

// デフォルトアニメ取得
const ufbx_anim* UfbxStaticModel::GetDefaultAnim() const
{
    return scene_ ? scene_.get()->anim : nullptr;
}

// scene_ を使う簡易ラッパ
void UfbxStaticModel::UpdateSkeletonAtTime(double t_sec)
{
    // 内部で管理している scene_ と既存のオーバーロードを使うラッパ
    UpdateSkeletonAtTime(scene_.get(), t_sec);
}

//----------------------------------------------------------------
// シーン読み込み
// ・ufbx_load_file を呼び出すラッパ
// ・unique_ptr に ufbx_free_scene をセットして管理しやすくする
//----------------------------------------------------------------
bool UfbxStaticModel::LoadScene(
    const char* fbx_path,
    std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)>& scene_out)
{
    ufbx_error     err;
    ufbx_load_opts opts;

    std::memset(&err, 0, sizeof(err));
    std::memset(&opts, 0, sizeof(opts));

    // 左手系 / Y-up / Z-forward に変換
    // → DirectX の座標系に合わせるための設定
    opts.target_axes = ufbx_axes_left_handed_y_up;
    opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;

    // FBX ファイル読み込み
    ufbx_scene* raw_scene = ufbx_load_file(fbx_path, &opts, &err);
    if (!raw_scene) {
        return false;
    }

    // unique_ptr に生ポインタとデリータを設定
    scene_out = std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)>(
        raw_scene, ufbx_free_scene);
    return true;
}

//----------------------------------------------------------------
// Load（FBX 読み込みのメイン関数）
// ・このクラスを使う側は基本的にこれだけ呼べば良い
//----------------------------------------------------------------
bool UfbxStaticModel::Load(const char* fbx_path)
{
    // シーン読み込み
    if (!LoadScene(fbx_path, scene_)) {
        return false;
    }

    // スケルトン（ボーン情報）構築
    if (!BuildSkeletonFromScene(scene_.get())) {
        return false;
    }

    // メッシュ展開（頂点・インデックス生成）
    ExpandAllNodes(scene_.get());

    // 頂点／インデックスの GPU バッファ作成
    if (!CreateGpuBuffers()) {
        return false;
    }

    // エフェクト＆テクスチャの作成
    if (!CreateEffectsAndTextures(fbx_path, scene_.get())) {
        return false;
    }

    // CPU 側の一次頂点データを解放（冗長コピーを減らす）
    std::vector<VertexPNT2>().swap(mesh_.vertices_);

    return true;
}
