#pragma once

#include <DirectXMath.h>
#include <wrl/client.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include <Effects.h>
#include <CommonStates.h>

// ufbx
struct ufbx_scene;
struct ufbx_node;
struct ufbx_material;
struct ufbx_anim;

// Forward D3D
struct ID3D11Buffer;
struct ID3D11ShaderResourceView;
struct ID3D11InputLayout;

class UfbxStaticModel
{
public:
    struct VertexPNT2
    {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT3 nrm;
        DirectX::XMFLOAT2 uv;
    };

    struct VertexInfluence
    {
        uint16_t bone[4]{};
        float    weight[4]{};
    };

    struct MeshPart
    {
        const ufbx_material* mat = nullptr;
        uint32_t             start_index = 0;
        uint32_t             index_count = 0;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    };

    struct BoneInfo
    {
        const ufbx_node* node = nullptr;
        int              parent = -1;

        DirectX::XMFLOAT4X4 bind_world{};
        DirectX::XMFLOAT4X4 inv_bind_world{};
        DirectX::XMFLOAT4X4 geom_bind_world{};
        DirectX::XMFLOAT4X4 inv_geom_bind_world{};
    };

    // 骨関連
    struct SkeletonData
    {
        std::vector<BoneInfo>                      bones_;
        std::vector<DirectX::XMFLOAT4X4>           curr_world_;     // node_to_world(t)
        std::unordered_map<const ufbx_node*, uint16_t> bone_index_of_;
        std::vector<DirectX::XMMATRIX>             skin_mats_;      // スキン行列キャッシュ
        float                                      scene_radius_ = 1.0f; // デバッグ軸長用
    };

    UfbxStaticModel();

    // FBX 読み込み（デバイスなどは内部で Gfx から取得）
    bool Load(const char* fbx_path);

    // メッシュ描画（ctx / states / effect は内部で取得）
    void Draw(const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj);

    // 骨デバッグ描画
    void DrawSkeleton(const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj);

    // 骨デバッグ／アニメ
    bool BuildSkeletonFromScene(const ufbx_scene* scene);
    void UpdateSkeletonAtTime(const ufbx_scene* scene, double t_sec);
    void UpdateSkeletonAtTime(const ufbx_scene* scene, const ufbx_anim* anim, double t_sec);
    void UpdateSkeletonAtTime(const ufbx_scene* scene, size_t stack_index, double t_sec);
    void UpdateSkeletonAtTime(const ufbx_scene* scene, const std::string& stack_name, double t_sec);

    // シーン読み込み
    bool LoadScene(const char* fbx_path,
        std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)>& scene_out);

private:


    // メッシュ関連
    struct MeshData
    {
        std::vector<VertexPNT2>      vertices_;
        std::vector<uint32_t>        indices_;
        std::vector<MeshPart>        parts_;

        std::vector<VertexInfluence> influences_;
        std::vector<VertexPNT2>      bind_vertices_;
        std::vector<VertexPNT2>      skinned_vertices_;
    };

    // 描画関連
    struct DrawResources
    {
        // メッシュ描画
        Microsoft::WRL::ComPtr<ID3D11Buffer>       vb_;
        Microsoft::WRL::ComPtr<ID3D11Buffer>       ib_;
        Microsoft::WRL::ComPtr<ID3D11InputLayout>  layout_;
        std::unique_ptr<DirectX::CommonStates>     states_;
        std::unique_ptr<DirectX::BasicEffect>      fx_;

        // 骨デバッグ描画
        std::unique_ptr<DirectX::BasicEffect>      debug_fx_;
        Microsoft::WRL::ComPtr<ID3D11InputLayout>  debug_layout_;
        Microsoft::WRL::ComPtr<ID3D11Buffer>       bone_vb_;
        size_t                                     bone_vb_size_ = 0;
    };

    // 内部処理
    void ExpandAllNodes(const ufbx_scene* scene);
    bool CreateGpuBuffers();
    bool CreateEffectsAndTextures(const char* fbx_path,
        const ufbx_scene* scene);

    SkeletonData skeleton_;
    MeshData     mesh_;
    DrawResources draw_;
};
