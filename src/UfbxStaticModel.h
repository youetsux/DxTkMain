#pragma once
#include <vector>
#include <memory>
#include <cstdint>
#include <string>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <d3d11.h>
#include <unordered_map>
#include <Effects.h>
#include <CommonStates.h>



struct ufbx_scene;
struct ufbx_anim;
struct ufbx_node;
struct ufbx_mesh;
struct ufbx_material;
struct ufbx_texture;


// 頂点ごとに最大4本の影響を受ける
struct VertexInfluence {
    uint16_t bone[4];   // グローバル骨インデックス（後でパレット対応に切替可）
    float    weight[4]; // 正規化済み
};

// メッシュ単位のスキン情報（最小）
struct MeshSkinCPU {
    std::vector<VertexInfluence> influences; // 頂点数と同じ長さ
    std::vector<uint16_t>        bonePalette; // このメッシュが参照する骨（重複なし）
};

// 位置(float4) / 法線 / UV
struct VertexPNT2 {
    DirectX::XMFLOAT3 pos;  // SV_Position として float4
    DirectX::XMFLOAT3 nrm;
    DirectX::XMFLOAT2 uv;
};

class UfbxStaticModel
{
public:
    UfbxStaticModel();
    ~UfbxStaticModel()=default;


    struct MeshPart {
        const ufbx_material* mat = nullptr;
        uint32_t                             startIndex = 0;
        uint32_t                             indexCount = 0;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv; // Diffuse相当
    };
    struct Mesh {
        // 既存: VB/IB, parts, material, など…
        std::vector<VertexPNT2> bindVertices;    // 追加: 読み込み時に保存（不変）
        std::vector<VertexPNT2> skinnedVertices; // 追加: 毎フレームCPUで上書き
        MeshSkinCPU              skinCPU;        // 既に追加済みのはず（weights/palette）
    };


 
    // 骨情報（最小限）
    struct BoneInfo {
        const ufbx_node* node = nullptr;
        int                 parent = -1;

        DirectX::XMFLOAT4X4 bindWorld{};     // B_i
        DirectX::XMFLOAT4X4 invBindWorld{};  // B_i^{-1}

        // 参考：メッシュ（ジオメトリ）側のバインド時ワールド（必要に応じて）
        DirectX::XMFLOAT4X4 geomBindWorld{};
        DirectX::XMFLOAT4X4 invGeomBindWorld{};
    };

public:

    // クラス内（ボーン数に合わせる）
    std::vector<DirectX::XMMATRIX> m_skinMats; // world * invBindWorld を毎フレーム詰める

    // FBX読込（全メッシュ静的描画）
    bool Load(ID3D11Device* device, const char* fbxPath, DirectX::DX11::BasicEffect* fx);
    void Draw(ID3D11DeviceContext * ctx, const DirectX::XMMATRIX & world, const DirectX::XMMATRIX & view, const DirectX::XMMATRIX & proj,
              DirectX::DX11::CommonStates * states, DirectX::DX11::BasicEffect * fx);

    // 骨デバッグ／アニメ（スキニング実装前の可視化）
    bool BuildSkeletonFromScene(const ufbx_scene* scene);
    void UpdateSkeletonAtTime(const ufbx_scene* scene, double t_sec);
    void UpdateSkeletonAtTime(const ufbx_scene* scene, const ufbx_anim* anim, double t_sec);
    // 便利オーバーロード（任意）
    void UpdateSkeletonAtTime(const ufbx_scene* scene, size_t stackIndex, double t_sec);
    void UpdateSkeletonAtTime(const ufbx_scene* scene, std::string stackName, double t_sec);

    void DrawSkeleton(ID3D11DeviceContext* ctx,
        const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj);
    // 内部手続き
    bool LoadScene(const char* fbxPath,
        std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)>& sceneOut);
private:

    void ExpandAllNodes(const ufbx_scene* scene);
    bool CreateGpuBuffers(ID3D11Device* device);
    //bool CreateEffectsAndTextures(ID3D11Device* device,
    //    const char* fbxPath,
    //    const ufbx_scene* scene);
    bool CreateEffectsAndTextures(ID3D11Device * device, const char* fbxPath,
        const ufbx_scene * scene, DirectX::DX11::BasicEffect * fx);

private:
    // CPU 展開結果
    std::vector<VertexPNT2> m_vertices;
    std::vector<uint32_t>   m_indices;
    std::vector<MeshPart>   m_parts;

    // GPU リソース
    Microsoft::WRL::ComPtr<ID3D11Buffer>       m_vb;
    Microsoft::WRL::ComPtr<ID3D11Buffer>       m_ib;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_layout;
    //std::unique_ptr<DirectX::DX11::CommonStates> m_states;
    //std::unique_ptr<DirectX::DX11::BasicEffect>  m_fx;

    // 骨デバッグ描画
    std::vector<BoneInfo>        m_bones;     // 骨の基本情報
    std::vector<DirectX::XMFLOAT4X4> m_currWorld; // W_i(t)

    std::unique_ptr<DirectX::DX11::BasicEffect> m_debugFx;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>   m_debugLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer>        m_boneVB;
    size_t m_boneVBSize = 0;

    // 可視化スケール用
    float m_sceneRadius = 1.0f;


    std::vector<VertexPNT2>     m_bindVertices;    // バインド姿勢の頂点（m_verticesと同じ長さ）
    std::vector<VertexPNT2>     m_skinnedVertices; // CPUスキニングの作業用
    std::vector<VertexInfluence> m_influences;     // 頂点ごとのボーンウェイト（m_verticesと同じ長さ）
    // スケルトン構築時に埋める
    std::unordered_map<const ufbx_node*, uint16_t> m_boneIndexOf_; // node* → 0..BoneCount-1

};
