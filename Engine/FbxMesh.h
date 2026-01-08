
#pragma once



#include <memory>

#include <vector>

#include <unordered_map>

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



struct BakedMeshImportResult; // from BakedMeshImporter.h (forward decl)

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

        // For baked skinning: local bone index -> ufbx_node::element_id
        std::vector<uint32_t>         skin_bone_node_element_ids_;
    };

public:
    FbxMesh();
    ~FbxMesh();

    bool BuildFromScene(const ufbx_scene* scene,
        FbxSkeleton& skeleton,
        const char* fbx_path);

    // ------------------------------------------------------------
    // node 1�inode->mesh 1�j�����W�J���č\�z�i�}���`���b�V���Ή��p�j
    // ���ύX�_: FbxMeshGroup ����Ăԑz��̐V�KAPI
    // ------------------------------------------------------------
    bool BuildFromNode(const ufbx_scene* scene,
        const ufbx_node* node,
        FbxSkeleton& skeleton,
        const char* fbx_path);

    // Build mesh from baked importer result (static mesh, no skinning).
    // Step: Debug visualization path. Existing behavior unchanged unless called.
    bool BuildFromBaked(const BakedMeshImportResult& src, const char* fbx_path = nullptr);
    bool BuildFromBaked(const BakedMeshImportResult& src, const ufbx_scene* scene, const char* fbx_path = nullptr);

    void Draw(
        const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj,
        FbxSkeleton& skeleton);

    void ApplySkinCPU(const std::vector<DirectX::XMMATRIX>& skin_mats);

    void ApplyUniformScale(float s);

    // �A�N�Z�T
    MeshData& Data() { return mesh_; }
    const MeshData& Data() const { return mesh_; }
    BVolume& GetBV() { return bounds_; }
    const BVolume& GetBV() const { return bounds_; }
    bool HasSkinning() const { return has_skinning_; }

private:
    void EmitCorner(struct BuildContext& ctx,
        uint32_t corner,
        uint32_t vtx);

    void EmitTriangleFanFaces(struct BuildContext& ctx,
        const ufbx_mesh* mesh,
        const std::vector<uint32_t>& face_list);

    void ExpandSingleNodeImpl(
        const ufbx_node* node,
        const std::unordered_map<const ufbx_node*, uint16_t>& bone_index_map,
        bool apply_geo);

    void ExpandAllNodesImpl(
        const std::vector<const ufbx_node*>& nodes,
        const std::unordered_map<const ufbx_node*, uint16_t>& bone_index_map,
        bool apply_geo);

    void PrepareExpandImpl();

    MeshPart MakeMeshPartForMaterial(
        const ufbx_node* node,
        const ufbx_mesh* mesh,
        uint32_t mat_index) const;

    void ExpandMaterialGroupImpl(
        const ufbx_node* node,
        const ufbx_mesh* mesh,
        uint32_t mat_index,
        const std::vector<uint32_t>& face_list,
        const std::vector<VertexInfluence>& infl_per_vtx,
        const ufbx_vertex_vec2* base_uv,
        bool apply_geo);

    void CommitMeshPartIfNotEmpty(MeshPart& part);

    void ExpandAllMaterialGroupsImpl(
        const ufbx_node* node,
        const ufbx_mesh* mesh,
        const std::unordered_map<uint32_t, std::vector<uint32_t>>& faces_by_mat,
        const std::vector<VertexInfluence>& infl_per_vtx,
        const ufbx_vertex_vec2* base_uv,
        bool apply_geo);



    void PrepareSkinningForMeshImpl(
        const ufbx_mesh* mesh,
        const std::unordered_map<const ufbx_node*, uint16_t>& bone_index_map,
        std::vector<VertexInfluence>& out_infl_per_vtx);

    void FinalizeExpandImpl(FbxSkeleton& skeleton, bool write_scene_radius);

    void ExpandAllNodes(const ufbx_scene* scene,
        FbxSkeleton& skeleton);

    // ------------------------------------------------------------
    // node 1�����W�J�iBuildFromNode �p�j
    // ���ύX�_: �}���`���b�V���Ή��̂��ߒǉ�
    // ------------------------------------------------------------
    void ExpandNode(const ufbx_scene* scene,
        const ufbx_node* node,
        FbxSkeleton& skeleton);

    bool CreateGpuBuffers();
    bool CreateEffectsAndTextures(
        const char* fbx_path,
        const ufbx_scene* scene);

    // ------------------------------------------------------------
    // ------------------------------------------------------------
    // ExpandNode / ExpandAllNodes �̋��ʏ���
    //   - apply_geo: node->geometry_to_world �𒸓_/�@���ɓK�p���邩
    //   - write_scene_radius: skeleton.Data().scene_radius_ ���X�V���邩
    // ------------------------------------------------------------
    void ExpandNodesImpl(
        const ufbx_scene* scene,
        const std::vector<const ufbx_node*>& nodes,
        FbxSkeleton& skeleton,
        bool apply_geo,
        bool write_scene_radius);

    // Draw() ���H���������邽�߂̕⏕�i�����_�����j
       // �� private �����o�ɃA�N�Z�X���邽�߁AFbxMesh �� private ���\�b�h�Ƃ��Ē�`����
       // ------------------------------------------------------------
    bool ValidateDrawResources(ID3D11DeviceContext* ctx) const;

    void UpdateSkinningIfNeeded(
        ID3D11DeviceContext* ctx,
        FbxSkeleton& skeleton);

    void SetupPipelineForMeshDraw(
        ID3D11DeviceContext* ctx,
        const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj);

    void DrawPartsWithBlendRestore(ID3D11DeviceContext* ctx);

private:
    MeshData mesh_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vb_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> ib_;

    std::unique_ptr<DirectX::DX11::CommonStates> states_;
    std::unique_ptr<DirectX::DX11::BasicEffect>  fx_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>    layout_;

    BVolume bounds_;
    bool    has_skinning_ = false;
    bool    skin_bones_remapped_ = false;
    bool    force_checker_texture_ = false;
};

