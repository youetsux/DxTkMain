#include "FbxSkeleton.h"
#include "ufbx.h"
#include "UfbxUtil.h"
#include "Gfx.h"        // DrawDebug 実装時に使う想定（今は未使用）

#include <unordered_map>

//------------------------------------------------------------
// 内部ヘルパ（匿名名前空間）
//------------------------------------------------------------
namespace
{
    // ボーン登録ヘルパ
    // ・ufbx_node を SkeletonData に追加し、そのインデックスを返す
    // ・すでに登録されている場合は既存のインデックスを返す
    int AddBoneInternal(
        const ufbx_node* node,
        SkeletonData& skeleton,
        std::unordered_map<const ufbx_node*, int>& index_of)
    {
        // すでに登録済みかチェック
        auto it = index_of.find(node);
        if (it != index_of.end()) {
            return it->second;
        }

        // 新しいボーンとして追加
        int idx = static_cast<int>(skeleton.bones_.size());
        index_of.insert(std::make_pair(node, idx));

        BoneInfo info;
        info.node = node;
        skeleton.bones_.push_back(info);
        return idx;
    }
}


const std::vector<BoneInfo>& FbxSkeleton::Bones() const
{
    return data_.bones_;
}

const std::vector<DirectX::XMFLOAT4X4>& FbxSkeleton::CurrWorld() const
{
    return data_.curr_world_;
}

float FbxSkeleton::SceneRadius() const
{
    return data_.scene_radius_;
}

const std::unordered_map<const ufbx_node*, uint16_t>& FbxSkeleton::BoneIndexMap() const
{
    return data_.bone_index_of_;
}

const std::vector<DirectX::XMMATRIX>& FbxSkeleton::SkinMatrices() const
{
    return data_.skin_mats_;
}

std::vector<DirectX::XMMATRIX>& FbxSkeleton::SkinMatrices()
{
    return data_.skin_mats_;
}

bool FbxSkeleton::BuildFromScene(const ufbx_scene* scene)
{
    // いったん全部クリア
    data_.bones_.clear();
    data_.curr_world_.clear();
    data_.bone_index_of_.clear();
    data_.skin_mats_.clear();
    data_.scene_radius_ = 1.0f;

    if (!scene) {
        return false;
    }

    // ufbx_node* → ボーンインデックス の一時マップ
    std::unordered_map<const ufbx_node*, int> index_of;

    // 全ノードをループ
    for (size_t ni = 0; ni < scene->nodes.count; ++ni) {
        const ufbx_node* node = scene->nodes.data[ni];
        const ufbx_mesh* mesh = node->mesh;
        if (!mesh) continue; // メッシュを持たないノードはスキップ

        // メッシュについているスキンデフォーマを全て見る
        for (size_t si = 0; si < mesh->skin_deformers.count; ++si) {
            const ufbx_skin_deformer* skin = mesh->skin_deformers.data[si];

            // 各クラスター（ボーン＋頂点群）の情報を見る
            for (size_t ci = 0; ci < skin->clusters.count; ++ci) {
                const ufbx_skin_cluster* cl = skin->clusters.data[ci];
                if (!cl->bone_node) continue;

                // このクラスターのボーンを SkeletonData に登録
                int bi = AddBoneInternal(
                    cl->bone_node, data_, index_of);

                // ボーン側のバインド姿勢（ボーンのワールド行列）
                data_.bones_[bi].bind_world =
                    UfbxUtil::ToXMMatrix(cl->bind_to_world);

                // バインド姿勢の逆行列も作っておく
                {
                    DirectX::XMMATRIX B =
                        DirectX::XMLoadFloat4x4(&data_.bones_[bi].bind_world);
                    DirectX::XMMATRIX B_inv = DirectX::XMMatrixInverse(nullptr, B);
                    DirectX::XMStoreFloat4x4(
                        &data_.bones_[bi].inv_bind_world, B_inv);
                }

                // ジオメトリ → ボーン の変換行列
                data_.bones_[bi].geom_bind_world =
                    UfbxUtil::ToXMMatrix(cl->geometry_to_bone);

                // その逆行列
                {
                    DirectX::XMMATRIX G =
                        DirectX::XMLoadFloat4x4(&data_.bones_[bi].geom_bind_world);
                    DirectX::XMMATRIX G_inv = DirectX::XMMatrixInverse(nullptr, G);
                    DirectX::XMStoreFloat4x4(
                        &data_.bones_[bi].inv_geom_bind_world, G_inv);
                }

                // 親ボーンとの関係（親があればインデックスを調べてセット）
                if (cl->bone_node->parent) {
                    auto itp = index_of.find(cl->bone_node->parent);
                    if (itp != index_of.end()) {
                        data_.bones_[bi].parent = itp->second;
                    }
                }
            }
        }
    }

    // ボーンが 1 本も無ければ、そのまま true（メッシュだけのモデル対応）
    if (data_.bones_.empty()) {
        return true;
    }

    // 現在姿勢・スキン行列用配列をボーン数に合わせて確保
    data_.curr_world_.resize(data_.bones_.size());
    data_.skin_mats_.resize(data_.bones_.size());

    // ufbx_node* → uint16_t ボーン番号 のマップを作る
    for (size_t i = 0; i < data_.bones_.size(); ++i) {
        // 初期姿勢はバインド姿勢
        data_.curr_world_[i] = data_.bones_[i].bind_world;

        if (data_.bones_[i].node && i < 0x10000) {
            uint16_t i16 = static_cast<uint16_t>(i);
            data_.bone_index_of_[data_.bones_[i].node] = i16;
        }
    }

    return true;
}

void FbxSkeleton::UpdateAtTime(const ufbx_scene* scene, const ufbx_anim* anim, double t_sec)
{
    if (!scene || !anim) return;
    if (data_.bones_.empty()) return;

    // t をアニメーション時間の範囲にクランプ
    double t = t_sec;
    if (anim->time_end > anim->time_begin) {
        if (t < anim->time_begin) t = anim->time_begin;
        if (t > anim->time_end)   t = anim->time_end;
    }

    // 計算済みノードの結果をためるキャッシュ
    static std::unordered_map<const ufbx_node*, DirectX::XMFLOAT4X4> cache;
    cache.clear();
    data_.curr_world_.resize(data_.bones_.size());

    for (size_t i = 0; i < data_.bones_.size(); ++i) {
        const ufbx_node* node = data_.bones_[i].node;
        if (!node) continue;

        // ボーンに対応するノードのワールド行列を求める
        data_.curr_world_[i] =
            UfbxUtil::EvaluateNodeWorldRecursive(node, anim, t, cache);
    }
}

void FbxSkeleton::DrawDebug(const DirectX::XMMATRIX& world, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj)
{
}
