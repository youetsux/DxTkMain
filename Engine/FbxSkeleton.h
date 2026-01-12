#pragma once
#include <cstdint>
#include <vector>
#include <DirectXMath.h>
#include <unordered_map>

struct ufbx_scene;
struct ufbx_node;
struct ufbx_material;
struct ufbx_anim;

// ------------------------------------------------------------
// ボーン1本分の情報
// ・どの ufbx_node に対応しているか
// ・親ボーンのインデックス
// ・バインドポーズ（初期姿勢）の行列
// ------------------------------------------------------------
struct BoneInfo
{
    int element_id = -1;
    int parent = -1;

    // Source ufbx node pointer (valid while the owning ufbx_scene lives)
    const ufbx_node* node = nullptr;

    // Unique typed id for the node (stable within the scene)
    uint64_t node_typed_id = 0;

    // Bind matrices (world space)
    DirectX::XMFLOAT4X4 bind_world;           // cluster bind_to_world (fallback: node bind)
    DirectX::XMFLOAT4X4 node_bind_world;      // node_to_world at import/bind (rest)
    DirectX::XMFLOAT4X4 inv_bind_world;       // inverse(bind_world)

    // Geometry bind (mesh geometry space -> bone space)
    DirectX::XMFLOAT4X4 geom_bind_world;      // cluster geometry_to_bone (fallback: identity)
    DirectX::XMFLOAT4X4 inv_geom_bind_world;  // inverse(geom_bind_world)

    // Correction so that bind_world aligns with node_bind_world (identity if already aligned)
    DirectX::XMFLOAT4X4 bind_fix_world;

    BoneInfo()
    {
        using namespace DirectX;
        const XMMATRIX I = XMMatrixIdentity();
        XMStoreFloat4x4(&bind_world, I);
        XMStoreFloat4x4(&node_bind_world, I);
        XMStoreFloat4x4(&inv_bind_world, I);
        XMStoreFloat4x4(&geom_bind_world, I);
        XMStoreFloat4x4(&inv_geom_bind_world, I);
        XMStoreFloat4x4(&bind_fix_world, I);
    }
};

// ------------------------------------------------------------
// スケルトン全体のデータ
// ・ボーン配列
// ・現在のボーンのワールド行列（アニメーションで変化）
// ・ufbx のノード → ボーン番号へのマップ
// ・スキニング用の行列キャッシュ
// ------------------------------------------------------------
struct SkeletonData
{
    // 全ボーンの情報（配列のインデックス = ボーン番号）
    std::vector<BoneInfo> bones_;

    // 現在時刻 t におけるボーンのワールド行列（node_to_world(t)）
    std::vector<DirectX::XMFLOAT4X4> curr_world_;

    // 1フレーム内でのノードワールド行列評価キャッシュ（UpdateAtTime() 用）
    std::unordered_map<const ufbx_node*, DirectX::XMFLOAT4X4> node_world_cache_;

    // ufbx_node* からボーン番号（uint16_t）を引くための辞書
    std::unordered_map<const ufbx_node*, uint16_t> bone_index_of_;

    // element_id -> bone index (for baked skin bone remap)
    std::unordered_map<uint32_t, uint16_t> bone_index_of_element_id_;

    // CPU スキニングで使うスキン行列の配列（毎フレーム更新）
    std::vector<DirectX::XMMATRIX> skin_mats_;

    // シーンのおおよその大きさ（半径）
    // → ボーンのデバッグ描画で軸の長さを決めるために使う
    float scene_radius_ = 1.0f;
};

class FbxSkeleton
{
public:
    const SkeletonData& Data() const { return data_; }
    SkeletonData& Data() { return data_; }

    const std::vector<BoneInfo>& Bones() const;
    const std::vector<DirectX::XMFLOAT4X4>& CurrWorld() const;
    float SceneRadius() const;
    float& SceneRadius() { return data_.scene_radius_; }

    // 追加関数
    const std::unordered_map<const ufbx_node*, uint16_t>& BoneIndexMap() const;
    bool FindBoneIndexByElementId(uint32_t element_id, uint16_t& out_index) const;
    const std::vector<DirectX::XMMATRIX>& SkinMatrices() const;
    std::vector<DirectX::XMMATRIX>& SkinMatrices(); // 書き込みしたい場合用

public:
    bool BuildFromScene(const ufbx_scene* scene);
    void UpdateAtTime(const ufbx_scene* scene, const ufbx_anim* anim, double t_sec);

    // baked runtime 用：typed_id インデックスの node world 行列配列から curr_world_ を更新する
    void UpdateFromBakedWorldMatrices(const std::vector<DirectX::XMMATRIX>& node_world);

    void DrawDebug(const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj);

private:
    SkeletonData data_;
};
