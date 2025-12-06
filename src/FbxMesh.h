#pragma once
#include <vector>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <CommonStates.h>
#include <Effects.h>
#include "BoundingVolume.h"

using namespace DirectX;

struct ufbx_scene;
struct ufbx_node;
struct ufbx_material;
struct ufbx_texture;

class FbxSkeleton;

namespace DirectX
{
    namespace DX11 {
        class BasicEffect;
        class CommonStates;
    }
}

struct BuildContext;

class FbxMesh
{
public:
    // ------------------------------------------------------------
    // 頂点構造体 (Position / Normal / Texcoord)
    // ------------------------------------------------------------
    struct VertexPNT2
    {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT3 nrm;
        DirectX::XMFLOAT2 uv;
    };

    // ------------------------------------------------------------
    // ボーンインフルエンス (最大4本)
    // ------------------------------------------------------------
    struct VertexInfluence
    {
        uint16_t bone[4];
        float    weight[4];

        VertexInfluence()
        {
            for (int i = 0; i < 4; ++i) {
                bone[i] = 0;
                weight[i] = 0.0f;
            }
        }
    };

    // ------------------------------------------------------------
    // メッシュの一部分 (マテリアル1つ分)
    // ------------------------------------------------------------
    struct MeshPart
    {
        const ufbx_material* mat = nullptr;
        uint32_t             start_index = 0;
        uint32_t             index_count = 0;

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    };

    // ------------------------------------------------------------
    // メッシュの CPU 側データ
    // ------------------------------------------------------------
    struct MeshData
    {
        std::vector<VertexPNT2>      vertices_;         // ロード時のフラット頂点
        std::vector<uint32_t>        indices_;          // インデックス
        std::vector<MeshPart>        parts_;            // マテリアル毎のパート

        std::vector<VertexInfluence> influences_;       // 頂点ごとのボーン影響
        std::vector<VertexPNT2>      bind_vertices_;    // バインドポーズ頂点
        std::vector<VertexPNT2>      skinned_vertices_; // スキニング後頂点
    };


public:
    FbxMesh() = default;

    const BVolume& GetBV() const { return bounds_; }
    // CPU メッシュ展開 + GPU バッファ + エフェクト/テクスチャ作成をまとめて行う
    bool BuildFromScene(const ufbx_scene* scene,
        FbxSkeleton& skeleton,
        const char* fbx_path);

    // 描画 (必要なら CPU スキニングを行う)
    void Draw(const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj,
        FbxSkeleton& skeleton);

    // アクセサ
    const MeshData& Data() const { return mesh_; }
    MeshData& Data() { return mesh_; }

private:
    // 旧 ExpandAllNodes
    void ExpandAllNodes(const ufbx_scene* scene,
        FbxSkeleton& skeleton);

    // 旧 CreateGpuBuffers
    bool CreateGpuBuffers();

    // 旧 CreateEffectsAndTextures
    bool CreateEffectsAndTextures(const char* fbx_path,
        const ufbx_scene* scene);

    void EmitCorner(
        BuildContext& ctx,
        uint32_t corner,
        uint32_t vtx);

    void ApplySkinCPU(
        const std::vector<DirectX::XMMATRIX>& skin_mats);

    void ApplyUniformScale(float s);
private:
    BVolume bounds_;
    MeshData mesh_;

    // 描画リソース (旧 draw_ 相当のメッシュ側だけ)
    Microsoft::WRL::ComPtr<ID3D11Buffer>          vb_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>          ib_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>     layout_;
    std::unique_ptr<DirectX::BasicEffect>         fx_;
    std::unique_ptr<DirectX::CommonStates>        states_;
};
