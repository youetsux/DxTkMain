// FbxMesh.h
#pragma once

#include <memory>
#include <vector>
#include <string>
#include <cstdint>

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <CommonStates.h>
#include <Effects.h>

#include "BoundingVolume.h"
#include "FbxSkeleton.h"
#include "ufbx.h"


//======================================================================
// FbxMesh
//======================================================================
class FbxMesh
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

    struct MeshData
    {
        std::vector<VertexPNT2>        vertices_;
        std::vector<uint32_t>         indices_;
        std::vector<MeshPart>         parts_;

        std::vector<VertexInfluence>  influences_;
        std::vector<VertexPNT2>       bind_vertices_;
        std::vector<VertexPNT2>       skinned_vertices_;
    };

public:
    FbxMesh();
    ~FbxMesh();

    bool BuildFromScene(const ufbx_scene* scene,
        FbxSkeleton& skeleton,
        const char* fbx_path);

    // ------------------------------------------------------------
    // node 1個（node->mesh 1個）だけ展開して構築（マルチメッシュ対応用）
    // ★変更点: FbxMeshGroup から呼ぶ想定の新規API
    // ------------------------------------------------------------
    bool BuildFromNode(const ufbx_scene* scene,
        const ufbx_node* node,
        FbxSkeleton& skeleton,
        const char* fbx_path);

    void Draw(
        const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj,
        FbxSkeleton& skeleton);

    void ApplySkinCPU(const std::vector<DirectX::XMMATRIX>& skin_mats);

    void ApplyUniformScale(float s);

    // アクセサ
    MeshData& Data() { return mesh_; }
    const MeshData& Data() const { return mesh_; }

    BVolume& GetBV() { return bounds_; }
    const BVolume& GetBV() const { return bounds_; }

    bool HasSkinning() const { return has_skinning_; }

private:
    void EmitCorner(struct BuildContext& ctx,
        uint32_t corner,
        uint32_t vtx);

    void ExpandAllNodes(const ufbx_scene* scene,
        FbxSkeleton& skeleton);

    // ------------------------------------------------------------
    // node 1個だけ展開（BuildFromNode 用）
    // ★変更点: マルチメッシュ対応のため追加
    // ------------------------------------------------------------
    void ExpandNode(const ufbx_scene* scene,
        const ufbx_node* node,
        FbxSkeleton& skeleton);

    bool CreateGpuBuffers();
    bool CreateEffectsAndTextures(
        const char* fbx_path,
        const ufbx_scene* scene);

private:
    MeshData mesh_;

    Microsoft::WRL::ComPtr<ID3D11Buffer> vb_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> ib_;

    std::unique_ptr<DirectX::DX11::CommonStates> states_;
    std::unique_ptr<DirectX::DX11::BasicEffect>  fx_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>    layout_;

    BVolume bounds_;
    bool    has_skinning_ = false;
};
