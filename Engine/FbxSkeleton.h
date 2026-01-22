#pragma once
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
    const ufbx_node* node = nullptr; // このボーンに対応する ufbx のノード
    int              parent = -1;    // 親ボーンのインデックス（なければ -1）

    DirectX::XMFLOAT4X4 bind_world{};           // ボーンのバインド姿勢のワールド行列
    DirectX::XMFLOAT4X4 inv_bind_world{};       // 上の逆行列
    DirectX::XMFLOAT4X4 geom_bind_world{};      // ジオメトリ → ボーン の変換行列
    DirectX::XMFLOAT4X4 inv_geom_bind_world{};  // その逆行列
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

    // ufbx_node* からボーン番号（uint16_t）を引くための辞書
    std::unordered_map<const ufbx_node*, uint16_t> bone_index_of_;

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
    const std::vector<DirectX::XMMATRIX>& SkinMatrices() const;
    std::vector<DirectX::XMMATRIX>& SkinMatrices(); // 書き込みしたい場合用

public:
    bool BuildFromScene(const ufbx_scene* scene);
    void UpdateAtTime(const ufbx_scene* scene, const ufbx_anim* anim, double t_sec);
    void DrawDebug(const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj);

private:
    SkeletonData data_;
};
