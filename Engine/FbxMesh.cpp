// FbxMesh.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "FbxMesh.h"
#include "FbxSkeleton.h"

#include <filesystem>
#include <string>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cfloat>

#include <WICTextureLoader.h>
#include <Effects.h>
#include <CommonStates.h>

#include "Gfx.h"
#include "ufbx.h"
#include "UfbxUtil.h"

using Microsoft::WRL::ComPtr;

//------------------------------------------------------------
// rhpReLXg
//------------------------------------------------------------
// rhpReLXg
//------------------------------------------------------------
struct BuildContext
{
    const ufbx_mesh* mesh = nullptr;
    const ufbx_vertex_vec2* uvv = nullptr;
    const std::vector<FbxMesh::VertexInfluence>* infl_per_vtx = nullptr;

    // m[hʃbV transform ĂށiPꃁbV݊ identity ̂܂܁j
    DirectX::XMMATRIX geo = DirectX::XMMatrixIdentity();
};

namespace
{
    using namespace DirectX;
    namespace fs = std::filesystem;

    // ------------------------------------------------------------
    // {[EFCg̃\[gp
    // ------------------------------------------------------------
    bool CompareBoneWeightPair(
        const std::pair<uint16_t, float>& a,
        const std::pair<uint16_t, float>& b)
    {
        return a.second > b.second;
    }

    // ------------------------------------------------------------
    // XL\zi1 bVPʁj
    // ------------------------------------------------------------
    void BuildInfluencesForMesh(
        const ufbx_mesh* mesh,
        const std::unordered_map<const ufbx_node*, uint16_t>& bone_index_map,
        std::vector<FbxMesh::VertexInfluence>& infl_per_vtx)
    {
        infl_per_vtx.clear();

        if (!mesh || mesh->skin_deformers.count == 0) {
            return;
        }

        infl_per_vtx.assign(mesh->num_vertices, FbxMesh::VertexInfluence());

        const ufbx_skin_deformer* skin = mesh->skin_deformers.data[0];
        const ufbx_skin_cluster_list& clusters = skin->clusters;
        const ufbx_skin_vertex_list& vtx_list = skin->vertices;
        const ufbx_skin_weight_list& w_list = skin->weights;

        // Uu{[ԍ{EFCgṽXg߂Ă 4 {ɍi
        std::vector<std::vector<std::pair<uint16_t, float>>> acc(mesh->num_vertices);

        for (size_t v = 0; v < vtx_list.count; ++v) {
            const ufbx_skin_vertex sv = vtx_list.data[v];
            uint32_t begin = sv.weight_begin;
            uint32_t n = sv.num_weights;

            for (uint32_t k = 0; k < n; ++k) {
                const ufbx_skin_weight w = w_list.data[begin + k];
                uint32_t ci = w.cluster_index;
                if (ci >= clusters.count) continue;

                const ufbx_skin_cluster* cl = clusters.data[ci];
                if (!cl || !cl->bone_node) continue;

                auto it = bone_index_map.find(cl->bone_node);
                if (it == bone_index_map.end()) continue;

                acc[v].push_back(std::make_pair(it->second, (float)w.weight));
            }
        }

        // 4 {܂łɍiĐK
        for (size_t v = 0; v < acc.size(); ++v) {
            auto& list = acc[v];
            if (list.empty()) continue;

            std::sort(list.begin(), list.end(), CompareBoneWeightPair);

            if (list.size() > 4) {
                list.resize(4);
            }

            float sum = 0.0f;
            for (auto& p : list) sum += p.second;
            if (sum <= 0.0f) sum = 1.0f;

            FbxMesh::VertexInfluence vi;
            size_t n_infl = list.size();
            if (n_infl > 4) n_infl = 4;

            for (size_t i = 0; i < n_infl; ++i) {
                vi.bone[i] = list[i].first;
                vi.weight[i] = list[i].second / sum;
            }
            infl_per_vtx[v] = vi;
        }
    }

    // ------------------------------------------------------------
    // }eA^eNX`ɑΉ UV ZbgI
    // ------------------------------------------------------------
    const ufbx_vertex_vec2* ChooseUVSet(
        const ufbx_mesh* mesh,
        const ufbx_material* mat,
        const ufbx_vertex_vec2* base_uv)
    {
        if (!mesh) return base_uv;

        const ufbx_texture* tex_for_uv = UfbxUtil::GetDiffuseTexture(mat);
        const ufbx_vertex_vec2* uvv = base_uv;

        if (tex_for_uv && tex_for_uv->uv_set.length > 0) {
            const ufbx_vertex_vec2* alt =
                UfbxUtil::ResolveUVByName(mesh, tex_for_uv->uv_set);
            if (alt && alt->exists) {
                uvv = alt;
            }
        }
        return uvv;
    }
} // anonymous namespace

FbxMesh::FbxMesh() = default;
FbxMesh::~FbxMesh() = default;

//================================================================
// FbxMesh::EmitCorner
//================================================================
void FbxMesh::EmitCorner(
    BuildContext& ctx,
    uint32_t corner,
    uint32_t vtx)
{
    using namespace DirectX;

    const ufbx_mesh* mesh = ctx.mesh;

    // ʒu
    uint32_t pi = UfbxUtil::ValueIndexOf(mesh->vertex_position, corner, vtx);
    ufbx_vec3 p = mesh->vertex_position.values.data[pi];
    XMFLOAT3 P((float)p.x, (float)p.y, (float)p.z);

    // m[h transform Ăށim[hf΍j
    {
        DirectX::XMVECTOR pv = UfbxUtil::TransformPosition(P, ctx.geo);
        DirectX::XMStoreFloat3(&P, pv);
    }

    // AABB XV
    bounds_.WrapBox(P);

    // @
    XMFLOAT3 N(0.0f, 1.0f, 0.0f);
    if (mesh->vertex_normal.exists) {
        uint32_t ni2 = UfbxUtil::ValueIndexOf(mesh->vertex_normal, corner, vtx);
        ufbx_vec3 n = mesh->vertex_normal.values.data[ni2];

        double len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (len > 0.0) {
            double inv = 1.0 / len;
            N.x = (float)(n.x * inv);
            N.y = (float)(n.y * inv);
            N.z = (float)(n.z * inv);
        }
        // ノード transform を法線にも反映（平行移動は無視）
        if (ctx.mesh) {
            DirectX::XMVECTOR nv = UfbxUtil::TransformNormal(N, ctx.geo);
            nv = DirectX::XMVector3Normalize(nv);
            DirectX::XMStoreFloat3(&N, nv);
        }

    }

    // UViV ]j
    XMFLOAT2 T(0.0f, 0.0f);
    if (ctx.uvv && ctx.uvv->exists) {
        uint32_t ti = UfbxUtil::ValueIndexOf(*ctx.uvv, corner, vtx);
        ufbx_vec2 t = ctx.uvv->values.data[ti];
        T.x = (float)t.x;
        T.y = (float)(1.0 - t.y);
    }

    VertexPNT2 vtx_out;
    vtx_out.pos = P;
    vtx_out.nrm = N;
    vtx_out.uv = T;

    VertexInfluence vi{};
    if (ctx.infl_per_vtx &&
        !ctx.infl_per_vtx->empty() &&
        vtx < ctx.infl_per_vtx->size())
    {
        vi = (*ctx.infl_per_vtx)[vtx];
    }

    mesh_.influences_.push_back(vi);
    mesh_.indices_.push_back((uint32_t)mesh_.vertices_.size());
    mesh_.vertices_.push_back(vtx_out);
}

//================================================================
// BuildFromScene
//================================================================
bool FbxMesh::BuildFromScene(const ufbx_scene* scene,
    FbxSkeleton& skeleton,
    const char* fbx_path)
{
    if (!scene) return false;

    ExpandAllNodes(scene, skeleton);

    if (!CreateGpuBuffers()) {
        return false;
    }

    if (!CreateEffectsAndTextures(fbx_path, scene)) {
        return false;
    }

    // WJɎgtbg_͔jioCh^XL_͎cj
    mesh_.vertices_.clear();
    mesh_.vertices_.shrink_to_fit();

    return true;
}

//================================================================
// BuildFromNodeinode->mesh 1WJč\zj
//   ύX_: }`bVFBXΉ̂ߒǉ
//================================================================
bool FbxMesh::BuildFromNode(const ufbx_scene* scene,
    const ufbx_node* node,
    FbxSkeleton& skeleton,
    const char* fbx_path)
{
    if (!scene || !node || !node->mesh) return false;

    ExpandNode(scene, node, skeleton);

    if (!CreateGpuBuffers()) {
        return false;
    }

    if (!CreateEffectsAndTextures(fbx_path, scene)) {
        return false;
    }

    // WJɎgtbg_͔jioCh^XL_͎cj
    mesh_.vertices_.clear();
    mesh_.vertices_.shrink_to_fit();

    return true;
}

//================================================================
// ExpandNodeiCPUj: node 1_WJ
//   ύX_: ExpandAllNodes u1m[hŁvɕ
//================================================================
void FbxMesh::ExpandNode(const ufbx_scene* scene,
    const ufbx_node* node,
    FbxSkeleton& skeleton)
{
    using namespace DirectX;

    // SNA
    mesh_.vertices_.clear();
    mesh_.indices_.clear();
    mesh_.parts_.clear();
    mesh_.influences_.clear();
    mesh_.bind_vertices_.clear();
    mesh_.skinned_vertices_.clear();

    // oEfBO{bNX
    bounds_.Reset();

    // Skeleton ̃{[}bv
    const auto& bone_index_map = skeleton.Data().bone_index_of_;

    has_skinning_ = false;

    const ufbx_mesh* mesh = node->mesh;
    if (!mesh) return;

    if (mesh->skin_deformers.count > 0) {
        has_skinning_ = true;
    }

    // _Ƃ̃XL
    std::vector<VertexInfluence> infl_per_vtx;
    BuildInfluencesForMesh(mesh, bone_index_map, infl_per_vtx);

    // { UV ZbgiƂ肠 1 j
    const ufbx_vertex_vec2* base_uv = nullptr;
    if (mesh->vertex_uv.exists) {
        base_uv = &mesh->vertex_uv;
    }
    else if (mesh->uv_sets.count > 0 &&
        mesh->uv_sets.data[0].vertex_uv.exists)
    {
        base_uv = &mesh->uv_sets.data[0].vertex_uv;
    }

    // tFCX}eAƂɃO[v
    std::unordered_map<uint32_t, std::vector<uint32_t>> faces_by_mat;
    for (uint32_t fi = 0; fi < (uint32_t)mesh->faces.count; ++fi) {
        uint32_t mi =
            (mesh->face_material.count > 0) ?
            mesh->face_material.data[fi] : 0;
        faces_by_mat[mi].push_back(fi);
    }

    // e}eAƂ MeshPart Ē_WJ
    for (auto& kv : faces_by_mat) {
        uint32_t                     mat_index = kv.first;
        const std::vector<uint32_t>& face_list = kv.second;

        MeshPart part;
        part.mat = nullptr;
        part.start_index = (uint32_t)mesh_.indices_.size();

        // }eA node / mesh T
        {
            const ufbx_material* mat = nullptr;
            if (node && node->materials.count > mat_index &&
                node->materials.data[mat_index])
            {
                mat = node->materials.data[mat_index];
            }
            else if (mesh && mesh->materials.count > mat_index) {
                mat = mesh->materials.data[mat_index];
            }
            part.mat = mat;
        }

        // UV ZbgeNX`ɍ킹đIђ
        const ufbx_vertex_vec2* uvv =
            ChooseUVSet(mesh, part.mat, base_uv);

        // ̃}eAŎgReLXg
        BuildContext ctx;
        ctx.mesh = mesh;
        ctx.uvv = uvv;
        ctx.infl_per_vtx = &infl_per_vtx;

        {
            DirectX::XMFLOAT4X4 m = UfbxUtil::ToXMMatrix(node->geometry_to_world);
            ctx.geo = DirectX::XMLoadFloat4x4(&m);
        }

        {
            DirectX::XMFLOAT4X4 m = UfbxUtil::ToXMMatrix(node->geometry_to_world);
            ctx.geo = DirectX::XMLoadFloat4x4(&m);
        }

        // o^ꂽtFCXSĎOp`ɕĒ_
        for (uint32_t f_index : face_list) {
            const ufbx_face f = mesh->faces.data[f_index];
            if (f.num_indices < 3) continue;

            // np`u`vŎOp`ɂ
            for (uint32_t k = 0; k + 2 < f.num_indices; ++k) {
                uint32_t corners[3] = {
                    f.index_begin + 0,
                    f.index_begin + (k + 1),
                    f.index_begin + (k + 2),
                };

                for (int c = 0; c < 3; ++c) {
                    uint32_t corner = corners[c];
                    uint32_t vtx = mesh->vertex_indices.data[corner];

                    EmitCorner(ctx, corner, vtx);
                }
            }
        }

        //  MeshPart gCfbNXL^
        part.index_count =
            (uint32_t)mesh_.indices_.size() - part.start_index;
        if (part.index_count > 0) {
            mesh_.parts_.push_back(part);
        }
    }

    // AABB 狅XV
    bounds_.RecalcSphereFromAABB();

    // oCh|[Y_EXLjOʒ_
    mesh_.bind_vertices_ = mesh_.vertices_;
    mesh_.skinned_vertices_ = mesh_.vertices_;
}

//================================================================
// bVWJiCPUj
//================================================================
void FbxMesh::ExpandAllNodes(const ufbx_scene* scene,
    FbxSkeleton& skeleton)
{
    using namespace DirectX;

    // SNA
    mesh_.vertices_.clear();
    mesh_.indices_.clear();
    mesh_.parts_.clear();
    mesh_.influences_.clear();
    mesh_.bind_vertices_.clear();
    mesh_.skinned_vertices_.clear();

    // oEfBO{bNX
    bounds_.Reset();

    // Skeleton ̃{[}bv
    const auto& bone_index_map = skeleton.Data().bone_index_of_;

    has_skinning_ = false;
    // V[̑Sm[h`FbN
    for (size_t ni = 0; ni < scene->nodes.count; ++ni) {
        const ufbx_node* node = scene->nodes.data[ni];
        const ufbx_mesh* mesh = node->mesh;
        if (!mesh) continue;

        if (mesh->skin_deformers.count > 0) {
            has_skinning_ = true;
        }

        // _Ƃ̃XL
        std::vector<VertexInfluence> infl_per_vtx;
        BuildInfluencesForMesh(mesh, bone_index_map, infl_per_vtx);

        // { UV ZbgiƂ肠 1 j
        const ufbx_vertex_vec2* base_uv = nullptr;
        if (mesh->vertex_uv.exists) {
            base_uv = &mesh->vertex_uv;
        }
        else if (mesh->uv_sets.count > 0 &&
            mesh->uv_sets.data[0].vertex_uv.exists)
        {
            base_uv = &mesh->uv_sets.data[0].vertex_uv;
        }

        // tFCX}eAƂɃO[v
        std::unordered_map<uint32_t, std::vector<uint32_t>> faces_by_mat;
        for (uint32_t fi = 0; fi < (uint32_t)mesh->faces.count; ++fi) {
            uint32_t mi =
                (mesh->face_material.count > 0) ?
                mesh->face_material.data[fi] : 0;
            faces_by_mat[mi].push_back(fi);
        }

        // e}eAƂ MeshPart Ē_WJ
        for (auto& kv : faces_by_mat) {
            uint32_t                     mat_index = kv.first;
            const std::vector<uint32_t>& face_list = kv.second;

            MeshPart part;
            part.mat = nullptr;
            part.start_index = (uint32_t)mesh_.indices_.size();

            // }eA node / mesh T
            {
                const ufbx_material* mat = nullptr;
                if (node && node->materials.count > mat_index &&
                    node->materials.data[mat_index])
                {
                    mat = node->materials.data[mat_index];
                }
                else if (mesh && mesh->materials.count > mat_index) {
                    mat = mesh->materials.data[mat_index];
                }
                part.mat = mat;
            }

            // UV ZbgeNX`ɍ킹đIђ
            const ufbx_vertex_vec2* uvv =
                ChooseUVSet(mesh, part.mat, base_uv);

            // ̃}eAŎgReLXg
            BuildContext ctx;
            ctx.mesh = mesh;
            ctx.uvv = uvv;
            ctx.infl_per_vtx = &infl_per_vtx;

            // o^ꂽtFCXSĎOp`ɕĒ_
            for (uint32_t f_index : face_list) {
                const ufbx_face f = mesh->faces.data[f_index];
                if (f.num_indices < 3) continue;

                // np`u`vŎOp`ɂ
                for (uint32_t k = 0; k + 2 < f.num_indices; ++k) {
                    uint32_t corners[3] = {
                        f.index_begin + 0,
                        f.index_begin + (k + 1),
                        f.index_begin + (k + 2),
                    };

                    for (int c = 0; c < 3; ++c) {
                        uint32_t corner = corners[c];
                        uint32_t vtx = mesh->vertex_indices.data[corner];

                        EmitCorner(ctx, corner, vtx);
                    }
                }
            }

            //  MeshPart gCfbNXL^
            part.index_count =
                (uint32_t)mesh_.indices_.size() - part.start_index;
            if (part.index_count > 0) {
                mesh_.parts_.push_back(part);
            }
        }
    }

    // AABB 狅XV
    bounds_.RecalcSphereFromAABB();

    // XPgɃV[aށi{[\Ŏgpj
    skeleton.Data().scene_radius_ = bounds_.radius;

    // oCh|[Y_EXLjOʒ_
    mesh_.bind_vertices_ = mesh_.vertices_;
    mesh_.skinned_vertices_ = mesh_.vertices_;
}

//================================================================
// GPU obt@쐬
//================================================================
bool FbxMesh::CreateGpuBuffers()
{
    if (mesh_.vertices_.empty()) return false;

    ID3D11Device* device = Gfx::Dev();
    if (!device) return false;

    // VB 쐬
    {
        D3D11_BUFFER_DESC desc;
        std::memset(&desc, 0, sizeof(desc));
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.ByteWidth =
            (UINT)(mesh_.vertices_.size() * sizeof(VertexPNT2));
        desc.Usage = D3D11_USAGE_DEFAULT;

        D3D11_SUBRESOURCE_DATA init;
        std::memset(&init, 0, sizeof(init));
        init.pSysMem = &mesh_.vertices_[0];

        HRESULT hr = device->CreateBuffer(
            &desc, &init, vb_.GetAddressOf());
        if (FAILED(hr)) {
            return false;
        }
    }

    // IB 쐬
    {
        D3D11_BUFFER_DESC desc;
        std::memset(&desc, 0, sizeof(desc));
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        desc.ByteWidth =
            (UINT)(mesh_.indices_.size() * sizeof(uint32_t));
        desc.Usage = D3D11_USAGE_DEFAULT;

        D3D11_SUBRESOURCE_DATA init;
        std::memset(&init, 0, sizeof(init));
        init.pSysMem = &mesh_.indices_[0];

        HRESULT hr = device->CreateBuffer(
            &desc, &init, ib_.GetAddressOf());
        if (FAILED(hr)) {
            return false;
        }
    }

    return true;
}

//================================================================
// GtFNgeNX`쐬
//================================================================
bool FbxMesh::CreateEffectsAndTextures(
    const char* fbx_path,
    const ufbx_scene* /*scene*/)
{
    using namespace DirectX;
    namespace fs = std::filesystem;

    ID3D11Device* device = Gfx::Dev();
    ID3D11DeviceContext* ctx = Gfx::Ctx();
    if (!device || !ctx) return false;

    // CommonStates / BasicEffect ̊m
    if (!states_) {
        states_.reset(new DirectX::DX11::CommonStates(device));
    }
    if (!fx_) {
        fx_.reset(new DirectX::DX11::BasicEffect(device));
    }

    DirectX::BasicEffect* fx = fx_.get();

    // ȒPȃCeBOݒ
    fx->SetLightingEnabled(true);
    fx->SetPerPixelLighting(true);
    fx->SetVertexColorEnabled(false);
    fx->SetTextureEnabled(true);
    fx->SetAmbientLightColor({ 0.3f, 0.3f, 0.3f });
    fx->SetDiffuseColor({ 1.0f,  1.0f,  1.0f, 1.0f });
    fx->SetLightEnabled(0, true);
    fx->SetLightDirection(0, { -0.5f, -1.0f, 0.3f });
    fx->SetLightDiffuseColor(0, { 1.0f, 1.0f, 1.0f, 1.0f });

    // ̓CAEg
    if (!layout_) {
        const void* bc = nullptr;
        size_t      sz = 0;
        fx->GetVertexShaderBytecode(&bc, &sz);

        D3D11_INPUT_ELEMENT_DESC il[] =
        {
            { "SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
              (UINT)offsetof(VertexPNT2, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",      0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
              (UINT)offsetof(VertexPNT2, nrm), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT,    0,
              (UINT)offsetof(VertexPNT2, uv),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        HRESULT hr = device->CreateInputLayout(
            il, 3, bc, sz, layout_.GetAddressOf());
        if (FAILED(hr)) {
            return false;
        }
    }

    // FBX t@C̃fBNg
    fs::path fbx_dir;
    if (fbx_path) {
        size_t len = std::strlen(fbx_path);
        fbx_dir = UfbxUtil::PathFromUtf8(fbx_path, len).parent_path();
    }

    // e MeshPart ̃eNX`쐬
    for (size_t i = 0; i < mesh_.parts_.size(); ++i) {
        MeshPart& part = mesh_.parts_[i];

        const ufbx_texture* tex = UfbxUtil::GetDiffuseTexture(part.mat);
        if (!tex) continue;

        HRESULT hr = E_FAIL;

        // FBX ߍ
        if (tex->content.size > 0 && tex->content.data) {
            hr = DirectX::CreateWICTextureFromMemory(
                device,
                ctx,
                reinterpret_cast<const uint8_t*>(tex->content.data),
                tex->content.size,
                nullptr,
                part.srv.ReleaseAndGetAddressOf());
        }
        // Ot@C
        else if (tex->filename.length > 0 && tex->filename.data) {
            fs::path tex_path = fbx_dir / UfbxUtil::FileNameFromUfbx(tex->filename);
            if (fs::exists(tex_path)) {
                hr = DirectX::CreateWICTextureFromFile(
                    device,
                    ctx,
                    tex_path.wstring().c_str(),
                    nullptr,
                    part.srv.ReleaseAndGetAddressOf());
            }
        }

        if (FAILED(hr)) {
            part.srv.Reset();
        }
    }

    return true;
}

void FbxMesh::ApplySkinCPU(
    const std::vector<DirectX::XMMATRIX>& skin_mats)
{
    using namespace DirectX;

    const auto& influences = mesh_.influences_;
    const auto& bind_vertices = mesh_.bind_vertices_;
    auto& out_vertices = mesh_.skinned_vertices_;

    size_t n = bind_vertices.size();
    if (n == 0) return;

    if (out_vertices.size() != n) {
        out_vertices.resize(n);
    }
    if (influences.size() < n) {
        n = influences.size();
    }

    for (size_t v = 0; v < n; ++v) {
        const VertexInfluence& inf = influences[v];

        XMVECTOR P = XMVectorZero();
        XMVECTOR N = XMVectorZero();
        bool any = false;

        for (int k = 0; k < 4; ++k) {
            float    w = inf.weight[k];
            uint16_t b = inf.bone[k];

            if (w <= 0.0f) continue;
            if (b >= skin_mats.size()) continue;

            const XMMATRIX& B = skin_mats[b];
            XMVECTOR W = XMVectorReplicate(w);

            P = XMVectorMultiplyAdd(
                UfbxUtil::TransformPosition(bind_vertices[v].pos, B),
                W, P);

            N = XMVectorMultiplyAdd(
                UfbxUtil::TransformNormal(bind_vertices[v].nrm, B),
                W, N);

            any = true;
        }

        if (!any) {
            out_vertices[v] = bind_vertices[v];
            continue;
        }

        VertexPNT2 sv = bind_vertices[v];
        XMStoreFloat3(&sv.pos, P);
        N = XMVector3Normalize(N);
        XMStoreFloat3(&sv.nrm, N);
        out_vertices[v] = sv;
    }
}

//================================================================
// bV`
//================================================================
void FbxMesh::Draw(
    const DirectX::XMMATRIX& world,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& proj,
    FbxSkeleton& skeleton)
{
    using namespace DirectX;

    ID3D11DeviceContext* ctx = Gfx::Ctx();
    if (!ctx) return;

    if (!vb_ || !ib_)             return;
    if (!fx_ || !states_ || !layout_) return;
    if (mesh_.indices_.empty())   return;

    if (has_skinning_) {
        if (!mesh_.influences_.empty() && !mesh_.bind_vertices_.empty()) {
            auto& skin_mats = skeleton.SkinMatrices();
            skin_mats.resize(skeleton.Bones().size());

            for (size_t i = 0; i < skeleton.Bones().size(); ++i) {
                XMMATRIX W = XMLoadFloat4x4(&skeleton.CurrWorld()[i]);
                XMMATRIX G2B = XMLoadFloat4x4(&skeleton.Bones()[i].geom_bind_world);
                skin_mats[i] = XMMatrixMultiply(G2B, W);
            }

            ApplySkinCPU(skin_mats);

            ctx->UpdateSubresource(
                vb_.Get(), 0, nullptr,
                &mesh_.skinned_vertices_[0], 0, 0);
        }
    }

    UINT stride = sizeof(VertexPNT2);
    UINT offset = 0;
    ID3D11Buffer* vb = vb_.Get();

    ctx->IASetInputLayout(layout_.Get());
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(ib_.Get(), DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx->OMSetDepthStencilState(states_->DepthDefault(), 0);
    ctx->RSSetState(states_->CullCounterClockwise());

    ID3D11SamplerState* samp = states_->LinearClamp();
    ctx->PSSetSamplers(0, 1, &samp);

    fx_->SetWorld(world);
    fx_->SetView(view);
    fx_->SetProjection(proj);

    ID3D11BlendState* prev_blend = nullptr;
    FLOAT             prev_factor[4];
    UINT              prev_mask = 0xFFFFFFFF;
    ctx->OMGetBlendState(&prev_blend, prev_factor, &prev_mask);

    ID3D11BlendState* bound = prev_blend;

    for (size_t i_part = 0; i_part < mesh_.parts_.size(); ++i_part) {
        const MeshPart& part = mesh_.parts_[i_part];

        bool has_tex = (part.srv != nullptr);
        ID3D11BlendState* target =
            has_tex ? states_->NonPremultiplied()
            : states_->Opaque();

        if (bound != target) {
            ctx->OMSetBlendState(target, nullptr, 0xFFFFFFFF);
            bound = target;
        }

        fx_->SetTextureEnabled(has_tex);
        if (has_tex) {
            fx_->SetTexture(part.srv.Get());
        }

        fx_->Apply(ctx);
        ctx->DrawIndexed(part.index_count, part.start_index, 0);
    }

    if (prev_blend) {
        ctx->OMSetBlendState(prev_blend, prev_factor, prev_mask);
        prev_blend->Release();
    }
}

void FbxMesh::ApplyUniformScale(float s)
{
    if (s <= 0.0f) return;

    for (auto& v : mesh_.vertices_) {
        v.pos.x *= s; v.pos.y *= s; v.pos.z *= s;
    }
    for (auto& v : mesh_.bind_vertices_) {
        v.pos.x *= s; v.pos.y *= s; v.pos.z *= s;
    }
    for (auto& v : mesh_.skinned_vertices_) {
        v.pos.x *= s; v.pos.y *= s; v.pos.z *= s;
    }

    bounds_.Scale(s);
}
