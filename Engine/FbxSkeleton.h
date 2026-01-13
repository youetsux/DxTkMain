#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <DirectXMath.h>

struct ufbx_scene;
struct ufbx_node;
struct ufbx_anim;
struct ufbx_baked_anim;
struct ufbx_baked_node;

// ------------------------------------------------------------
// ボーン1本分の情報
// ------------------------------------------------------------
struct BoneInfo
{
    const ufbx_node* node = nullptr;
    int              parent = -1;

    DirectX::XMFLOAT4X4 bind_world{};
    DirectX::XMFLOAT4X4 inv_bind_world{};
    DirectX::XMFLOAT4X4 geom_bind_world{};
    DirectX::XMFLOAT4X4 inv_geom_bind_world{};
};

// ------------------------------------------------------------
// スケルトン全体のデータ
// ------------------------------------------------------------
struct SkeletonData
{
    std::vector<BoneInfo> bones_;
    std::vector<DirectX::XMFLOAT4X4> curr_world_;
    std::unordered_map<const ufbx_node*, DirectX::XMFLOAT4X4> node_world_cache_;
    std::unordered_map<const ufbx_node*, uint16_t> bone_index_of_;
    std::vector<DirectX::XMMATRIX> skin_mats_;
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

    const std::unordered_map<const ufbx_node*, uint16_t>& BoneIndexMap() const;
    const std::vector<DirectX::XMMATRIX>& SkinMatrices() const;
    std::vector<DirectX::XMMATRIX>& SkinMatrices();

public:
    bool BuildFromScene(const ufbx_scene* scene);
    void UpdateAtTime(const ufbx_scene* scene, const ufbx_anim* anim, double t_sec);
    void DrawDebug(const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj);

private:
    SkeletonData data_;
};
