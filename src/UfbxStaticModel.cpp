#define NOMINMAX
#include "UfbxStaticModel.h"
#include <Windows.h>
#include <cassert>
#include <filesystem>
#include <unordered_map>
#include <string>
#include <cstring>              // std::strlen, std::strncmp
#include <cmath>                // std::sqrt, std::max
#include <algorithm>
#include <DirectXMath.h>

#include <WICTextureLoader.h>   // DirectXTK
#include <Effects.h>
#include <CommonStates.h>

// ufbx
#include "ufbx.h"

using Microsoft::WRL::ComPtr;

namespace {
    using namespace DirectX;
    namespace DXTK = DirectX::DX11;
    namespace fs = std::filesystem;

    struct DebugVC {
        XMFLOAT3 pos;
        XMFLOAT4 col;
    };

    //==============================================================
    // ufbx_scene 解放用関数（unique_ptr のカスタムデリータ用）
    //==============================================================
    inline void FreeUfbxScene(ufbx_scene* s)
    {
        if (s) ufbx_free_scene(s);
    }

    //==============================================================
    // UTF-8 → std::filesystem::path（C++20）
    //==============================================================
    inline fs::path PathFromUtf8(const char* bytes, size_t len)
    {
        const char8_t* p = reinterpret_cast<const char8_t*>(bytes);
        return fs::path(std::u8string(p, p + len));
    }

    inline fs::path PathFromUfbx(const ufbx_string& s)
    {
        if (!s.data || s.length == 0) return {};
        return PathFromUtf8(s.data, s.length);
    }

    inline fs::path FileNameFromUfbx(const ufbx_string& s)
    {
        return PathFromUfbx(s).filename();
    }

    //==============================================================
    // マテリアル → Diffuse 相当テクスチャ（PBR優先）
    //==============================================================
    inline const ufbx_texture* GetDiffuseTexture(const ufbx_material* mat)
    {
        if (!mat) return nullptr;
        if (mat->pbr.base_color.texture)    return mat->pbr.base_color.texture;
        if (mat->fbx.diffuse_color.texture) return mat->fbx.diffuse_color.texture;
        if (mat->fbx.ambient_color.texture) return mat->fbx.ambient_color.texture;
        return nullptr;
    }

    //==============================================================
    // ufbx_string 比較（完全一致）
    //==============================================================
    inline bool ufbx_string_eq(const ufbx_string& a, const ufbx_string& b)
    {
        if (a.length != b.length || !a.data || !b.data) return false;
        return std::strncmp(a.data, b.data, a.length) == 0;
    }

    //==============================================================
    // UVセット名から vertex_uv を解決
    //==============================================================
    inline const ufbx_vertex_vec2* ResolveUVByName(
        const ufbx_mesh* mesh,
        const ufbx_string& uv_set_name)
    {
        if (!mesh || uv_set_name.length == 0) return nullptr;
        for (size_t i = 0; i < mesh->uv_sets.count; ++i) {
            const ufbx_uv_set& us = mesh->uv_sets.data[i];
            if (us.vertex_uv.exists && ufbx_string_eq(us.name, uv_set_name)) {
                return &us.vertex_uv;
            }
        }
        return nullptr;
    }

    //==============================================================
    // ufbx の頂点属性 indices/unique_per_vertex に従って値インデックス決定
    //==============================================================
    template<typename AttrT>
    inline uint32_t ValueIndexOf(const AttrT& attr, uint32_t corner, uint32_t vtx)
    {
        if (attr.indices.count > 0)  return attr.indices.data[corner];
        if (attr.unique_per_vertex)  return vtx;
        return corner;
    }

    //==============================================================
    // ufbx_matrix(列ベース3x4) → XMFLOAT4X4（行ベース4x4）
    //==============================================================
    static XMFLOAT4X4 ToXMMatrix(const ufbx_matrix& m)
    {
        return {
            (float)m.m00, (float)m.m10, (float)m.m20, 0.0f,
            (float)m.m01, (float)m.m11, (float)m.m21, 0.0f,
            (float)m.m02, (float)m.m12, (float)m.m22, 0.0f,
            (float)m.m03, (float)m.m13, (float)m.m23, 1.0f,
        };
    }

    inline XMVECTOR TPos(const XMFLOAT3& p, const XMMATRIX& M)
    {
        return XMVector3Transform(XMLoadFloat3(&p), M);
    }

    inline XMVECTOR TNrm(const XMFLOAT3& n, const XMMATRIX& M)
    {
        return XMVector3TransformNormal(XMLoadFloat3(&n), M);
    }

    //==============================================================
    // CPU スキニング
    //==============================================================
    void ApplySkinCPU(
        const std::vector<XMMATRIX>& skinMats,
        const std::vector<VertexInfluence>& infls,
        const std::vector<VertexPNT2>& bindVtx,
        std::vector<VertexPNT2>& outVtx)
    {
        const size_t n = bindVtx.size();
        outVtx.resize(n);

        for (size_t v = 0; v < n; ++v) {
            const auto& bi = infls[v];

            XMVECTOR P = XMVectorZero();
            XMVECTOR N = XMVectorZero();
            bool any = false;

            for (int k = 0; k < 4; ++k) {
                const float    w = bi.weight[k];
                const uint16_t b = bi.bone[k];
                if (w <= 0.0f || b >= skinMats.size()) continue;

                const XMMATRIX& B = skinMats[b];
                const XMVECTOR  W = XMVectorReplicate(w);

                P = XMVectorMultiplyAdd(TPos(bindVtx[v].pos, B), W, P);
                N = XMVectorMultiplyAdd(TNrm(bindVtx[v].nrm, B), W, N);
                any = true;
            }

            if (!any) {
                outVtx[v] = bindVtx[v];
                continue;
            }

            VertexPNT2 sv = bindVtx[v];
            XMStoreFloat3(&sv.pos, P);
            N = XMVector3Normalize(N);
            XMStoreFloat3(&sv.nrm, N);
            outVtx[v] = sv;
        }
    }

    //==============================================================
    // 指定インデックスのマテリアル解決（node優先→mesh）
    //==============================================================
    const ufbx_material* GetMaterialByIndex(
        const ufbx_node* node,
        const ufbx_mesh* mesh,
        uint32_t mi)
    {
        if (node && node->materials.count > mi && node->materials.data[mi]) {
            return node->materials.data[mi];
        }
        if (mesh && mesh->materials.count > mi) {
            return mesh->materials.data[mi];
        }
        return nullptr;
    }

    //==============================================================
    // 骨追加ヘルパ（ラムダの代わり）
    //   indexOf: node → bone index マップ
    //   bones  : BoneInfo 配列（UfbxStaticModel のメンバを想定）
    //   n      : 追加したい ufbx_node*
    //==============================================================
    template<class TBoneInfo>
    int AddBoneHelper(
        std::unordered_map<const ufbx_node*, int>& indexOf,
        std::vector<TBoneInfo>& bones,
        const ufbx_node* n)
    {
        auto it = indexOf.find(n);
        if (it != indexOf.end()) return it->second;

        int idx = static_cast<int>(bones.size());
        indexOf[n] = idx;

        TBoneInfo bi{};
        bi.node = n;
        bones.push_back(bi);
        return idx;
    }

    //==============================================================
    // node_to_world(t) 再帰計算ヘルパ（ラムダの代わり）
    //==============================================================
    XMFLOAT4X4 EvaluateNodeWorldRecursive(
        const ufbx_node* node,
        const ufbx_anim* anim,
        double t,
        std::unordered_map<const ufbx_node*, XMFLOAT4X4>& cache)
    {
        auto it = cache.find(node);
        if (it != cache.end()) {
            return it->second;
        }

        // このノードのローカル変換を評価
        ufbx_transform xf = ufbx_evaluate_transform(anim, node, t);
        ufbx_matrix    lm = ufbx_transform_to_matrix(&xf);
        XMFLOAT4X4     xmLocal = ToXMMatrix(lm);
        XMMATRIX       L = XMLoadFloat4x4(&xmLocal);

        XMMATRIX W;
        if (const ufbx_node* parent = node->parent) {
            XMFLOAT4X4 xmParent = EvaluateNodeWorldRecursive(parent, anim, t, cache);
            XMMATRIX   PW = XMLoadFloat4x4(&xmParent);
            W = L * PW;    // parent_world * local（左手系・行ベクトル）
        }
        else {
            W = L;
        }

        XMFLOAT4X4 xmWorld;
        XMStoreFloat4x4(&xmWorld, W);
        cache.insert({ node, xmWorld });
        return xmWorld;
    }

    //==============================================================
    // 行列から平行移動成分だけ取り出す（ラムダ getPos の代わり）
    //==============================================================
    inline XMFLOAT3 GetPositionFromMatrix(const XMFLOAT4X4& M)
    {
        return { M._41, M._42, M._43 };
    }

    //==============================================================
    // 行列で点を変換（ラムダ mulPoint の代わり）
    //==============================================================
    inline XMFLOAT3 TransformPointByMatrix(const XMFLOAT4X4& M, const XMFLOAT3& v)
    {
        XMMATRIX m = XMLoadFloat4x4(&M);
        XMVECTOR p = XMVector3Transform(XMLoadFloat3(&v), m);
        XMFLOAT3 o;
        XMStoreFloat3(&o, p);
        return o;
    }

} // namespace

//==================================================================
// Scene 読み込み
//==================================================================
bool UfbxStaticModel::LoadScene(
    const char* fbxPath,
    std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)>& sceneOut)
{
    ufbx_error     err{};
    ufbx_load_opts opts{};

    // 左手系・Y up・Z forward へ揃える
    opts.target_axes = ufbx_axes_left_handed_y_up;
    opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;

    if (ufbx_scene* raw = ufbx_load_file(fbxPath, &opts, &err)) {
        sceneOut.reset(raw);
        return true;
    }
    return false;
}

//==================================================================
// CPU 展開：全ノードの全メッシュを三角形リストに
//   ★ ついでに AABB を取り、m_sceneRadius を計算
//==================================================================
void UfbxStaticModel::ExpandAllNodes(const ufbx_scene* scene)
{
    m_vertices.clear();
    m_indices.clear();
    m_parts.clear();
    m_influences.clear();

    for (size_t ni = 0; ni < scene->nodes.count; ++ni) {
        const ufbx_node* node = scene->nodes.data[ni];
        const ufbx_mesh* mesh = node->mesh;
        if (!mesh) continue;

        // 頂点ごとのスキン情報（最大4本）
        std::vector<VertexInfluence> inflPerVtx;
        if (mesh->skin_deformers.count > 0) {
            inflPerVtx.assign(mesh->num_vertices, VertexInfluence{ {0,0,0,0}, {0,0,0,0} });

            const ufbx_skin_deformer* skin = mesh->skin_deformers.data[0];
            const auto& clusters = skin->clusters;
            const auto& vtxList = skin->vertices;
            const auto& wList = skin->weights;

            std::vector<std::vector<std::pair<uint16_t, float>>> acc(mesh->num_vertices);

            for (size_t v = 0; v < vtxList.count; ++v) {
                const ufbx_skin_vertex sv = vtxList.data[v];
                const uint32_t begin = sv.weight_begin;
                const uint32_t n = sv.num_weights;

                for (uint32_t k = 0; k < n; ++k) {
                    const ufbx_skin_weight w = wList.data[begin + k];
                    const uint32_t         ci = w.cluster_index;
                    if (ci >= clusters.count) continue;

                    const ufbx_skin_cluster* cl = clusters.data[ci];
                    const ufbx_node* bone_node = cl ? cl->bone_node : nullptr;
                    if (!bone_node) continue;

                    auto it = m_boneIndexOf_.find(bone_node);
                    if (it == m_boneIndexOf_.end()) continue;

                    acc[v].emplace_back(it->second, (float)w.weight);
                }
            }

            // 上位4本に絞って正規化
            for (size_t v = 0; v < acc.size(); ++v) {
                auto& list = acc[v];
                if (list.empty()) continue;

                std::sort(list.begin(), list.end(),
                    [](auto& a, auto& b) { return a.second > b.second; });

                if (list.size() > 4) list.resize(4);

                float sum = 0.f;
                for (auto& p : list) sum += p.second;
                if (sum <= 0.f) sum = 1.f;

                VertexInfluence vi{};
                for (size_t i = 0; i < list.size(); ++i) {
                    vi.bone[i] = list[i].first;
                    vi.weight[i] = list[i].second / sum;
                }
                inflPerVtx[v] = vi;
            }
        }

        // 基本UV（mesh直下 → 無ければ最初のUVセット）
        const ufbx_vertex_vec2* baseUV = nullptr;
        if (mesh->vertex_uv.exists) {
            baseUV = &mesh->vertex_uv;
        }
        else if (mesh->uv_sets.count > 0 && mesh->uv_sets.data[0].vertex_uv.exists) {
            baseUV = &mesh->uv_sets.data[0].vertex_uv;
        }

        // フェイスをマテリアルごとに分類
        std::unordered_map<uint32_t, std::vector<uint32_t>> facesByMat;
        for (uint32_t fi = 0; fi < (uint32_t)mesh->faces.count; ++fi) {
            uint32_t mi = (mesh->face_material.count > 0)
                ? mesh->face_material.data[fi]
                : 0;
            facesByMat[mi].push_back(fi);
        }

        // マテリアルバケツ単位に展開
        for (auto& kv : facesByMat) {
            const uint32_t matIndex = kv.first;
            const auto& faceList = kv.second;

            MeshPart part{};
            part.mat = GetMaterialByIndex(node, mesh, matIndex);
            part.startIndex = (uint32_t)m_indices.size();

            const ufbx_texture* texForUV = GetDiffuseTexture(part.mat);
            const ufbx_vertex_vec2* uvv = baseUV;
            if (texForUV && texForUV->uv_set.length > 0) {
                if (const auto* alt = ResolveUVByName(mesh, texForUV->uv_set)) {
                    uvv = alt;
                }
            }

            for (uint32_t fi : faceList) {
                const ufbx_face f = mesh->faces.data[fi];
                if (f.num_indices < 3) continue;

                for (uint32_t k = 0; k + 2 < f.num_indices; ++k) {
                    const uint32_t corners[3] = {
                        f.index_begin + 0,
                        f.index_begin + (k + 1),
                        f.index_begin + (k + 2)
                    };

                    for (int c = 0; c < 3; ++c) {
                        const uint32_t corner = corners[c];
                        const uint32_t vtx = mesh->vertex_indices.data[corner];

                        // 位置
                        const uint32_t pi = ValueIndexOf(mesh->vertex_position, corner, vtx);
                        const ufbx_vec3 p = mesh->vertex_position.values.data[pi];
                        XMFLOAT3 P = { (float)p.x, (float)p.y, (float)p.z };

                        // 法線
                        XMFLOAT3 N = { 0,1,0 };
                        if (mesh->vertex_normal.exists) {
                            const uint32_t ni = ValueIndexOf(mesh->vertex_normal, corner, vtx);
                            const ufbx_vec3 n = mesh->vertex_normal.values.data[ni];
                            double len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
                            if (len > 0.0) {
                                N = { (float)(n.x / len), (float)(n.y / len), (float)(n.z / len) };
                            }
                        }

                        // UV（V 反転）
                        XMFLOAT2 T = { 0,0 };
                        if (uvv && uvv->exists) {
                            const uint32_t ti = ValueIndexOf(*uvv, corner, vtx);
                            const ufbx_vec2 t = uvv->values.data[ti];
                            T = { (float)t.x, (float)(1.0 - t.y) };
                        }

                        VertexPNT2 v{};
                        v.pos = P; v.nrm = N; v.uv = T;

                        VertexInfluence vi{};
                        if (!inflPerVtx.empty()) vi = inflPerVtx[vtx];
                        m_influences.push_back(vi);

                        m_indices.push_back((uint32_t)m_vertices.size());
                        m_vertices.push_back(v);
                    }
                }
            }

            part.indexCount = (uint32_t)m_indices.size() - part.startIndex;
            if (part.indexCount > 0)
                m_parts.push_back(std::move(part));
        }
    }

    m_bindVertices = m_vertices;
    m_skinnedVertices = m_vertices;
}



//==================================================================
// GPU バッファ
//==================================================================
bool UfbxStaticModel::CreateGpuBuffers(ID3D11Device* device)
{
    if (m_vertices.empty()) return false;

    // VB
    D3D11_BUFFER_DESC vbd{};
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.ByteWidth = (UINT)(m_vertices.size() * sizeof(VertexPNT2));
    vbd.Usage = D3D11_USAGE_DEFAULT;

    D3D11_SUBRESOURCE_DATA vsd{};
    vsd.pSysMem = m_vertices.data();

    if (FAILED(device->CreateBuffer(&vbd, &vsd, m_vb.GetAddressOf())))
        return false;

    // IB
    D3D11_BUFFER_DESC ibd{};
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.ByteWidth = (UINT)(m_indices.size() * sizeof(uint32_t));
    ibd.Usage = D3D11_USAGE_DEFAULT;

    D3D11_SUBRESOURCE_DATA isd{};
    isd.pSysMem = m_indices.data();

    if (FAILED(device->CreateBuffer(&ibd, &isd, m_ib.GetAddressOf())))
        return false;

    return true;
}

//==================================================================
// エフェクト＆テクスチャ（埋め込み → 同フォルダ）
//==================================================================
bool UfbxStaticModel::CreateEffectsAndTextures(
    ID3D11Device* device,
    const char* fbxPath,
    const ufbx_scene*,
    DXTK::
    * fx)
{
    const fs::path fbxDir = PathFromUtf8(fbxPath, std::strlen(fbxPath)).parent_path();

    // 固定設定は一度だけ
    fx->SetLightingEnabled(true);
    fx->SetPerPixelLighting(true);
    fx->SetVertexColorEnabled(false);
    fx->SetTextureEnabled(true);
    fx->SetAmbientLightColor({ 0.25f, 0.25f, 0.30f });
    fx->SetDiffuseColor({ 0.90f, 0.90f, 0.95f, 1.0f });
    fx->SetLightEnabled(0, true);
    fx->SetLightDirection(0, { -0.5f, -1.0f, -0.3f });
    fx->SetLightDiffuseColor(0, { 1.0f, 1.0f, 1.0f });

    // 入力レイアウトは fx の VS バイトコードから作成して保持
    const void* bc = nullptr;
    size_t      sz = 0;
    fx->GetVertexShaderBytecode(&bc, &sz);

    D3D11_INPUT_ELEMENT_DESC desc[] = {
        { "SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
            (UINT)offsetof(VertexPNT2, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",      0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
            (UINT)offsetof(VertexPNT2, nrm), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT,    0,
            (UINT)offsetof(VertexPNT2, uv),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    if(FAILED(device->CreateInputLayout(desc, _countof(desc), bc, sz, m_layout.ReleaseAndGetAddressOf())))
        return false;

    // テクスチャ（埋め込み → 同フォルダ）
    for (auto& part : m_parts) {
        const ufbx_texture* tex = GetDiffuseTexture(part.mat);
        if (!tex) continue;

        HRESULT hr = E_FAIL;
        if (tex->content.size > 0) {
            hr = DirectX::CreateWICTextureFromMemory(
                device,
                reinterpret_cast<const uint8_t*>(tex->content.data),
                tex->content.size,
                nullptr,
                part.srv.ReleaseAndGetAddressOf());
        }
        else if (tex->filename.length > 0) {
            fs::path texPath = fbxDir / FileNameFromUfbx(tex->filename);
            if (fs::exists(texPath)) {
                hr = DirectX::CreateWICTextureFromFile(
                    device,
                    texPath.wstring().c_str(),
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

UfbxStaticModel::UfbxStaticModel()
{
}

//==================================================================
// Load（まとめ）
//==================================================================
bool UfbxStaticModel::Load(
    ID3D11Device* device,
    const char* fbxPath,
    DXTK::BasicEffect* fx)
{
    std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)> scene(
        nullptr, &FreeUfbxScene);

    if (!LoadScene(fbxPath, scene))            return false;
    if (!BuildSkeletonFromScene(scene.get()))  return false;

    ExpandAllNodes(scene.get());

    if (!CreateGpuBuffers(device))             return false;
    if (!CreateEffectsAndTextures(device, fbxPath, scene.get(), fx)) return false;

    return true;
}

//==================================================================
// Draw（テクスチャ有→NonPremultiplied / 無→Opaque）
//==================================================================
void UfbxStaticModel::Draw(
    ID3D11DeviceContext* ctx,
    const XMMATRIX& world,
    const XMMATRIX& view,
    const XMMATRIX& proj,
    DXTK::CommonStates* states,
    DXTK::BasicEffect* fx)
{
    using namespace DirectX;

    if (!m_vb || !m_ib || !fx || !m_layout || m_indices.empty())
        return;

    // スキンがあれば CPU スキンを実行
    if (!m_influences.empty() && !m_bindVertices.empty()) {
        static std::vector<XMMATRIX> skinMats;
        skinMats.resize(m_bones.size());

        // skin = geometry_to_bone * bone_world(t)
        for (size_t i = 0; i < m_bones.size(); ++i) {
            const XMMATRIX W = XMLoadFloat4x4(&m_currWorld[i]);      // bone node_to_world(t)
            const XMMATRIX G2B = XMLoadFloat4x4(&m_bones[i].geomBindWorld); // geometry_to_bone
            skinMats[i] = XMMatrixMultiply(G2B, W);
        }

        ApplySkinCPU(skinMats, m_influences, m_bindVertices, m_skinnedVertices);

        ctx->UpdateSubresource(m_vb.Get(), 0, nullptr, m_skinnedVertices.data(), 0, 0);
    }

    UINT          stride = sizeof(VertexPNT2);
    UINT          offset = 0;
    ID3D11Buffer* vb = m_vb.Get();

    ctx->IASetInputLayout(m_layout.Get());
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(m_ib.Get(), DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx->OMSetDepthStencilState(states->DepthDefault(), 0);
    ctx->RSSetState(states->CullCounterClockwise());

    ID3D11SamplerState* samp = states->LinearClamp();
    ctx->PSSetSamplers(0, 1, &samp);

    fx->SetWorld(world);
    fx->SetView(view);
    fx->SetProjection(proj);

    ID3D11BlendState* prevBlend = nullptr;
    FLOAT             prevFactor[4] = {};
    UINT              prevMask = 0xFFFFFFFF;
    ctx->OMGetBlendState(&prevBlend, prevFactor, &prevMask);

    ID3D11BlendState* bound = prevBlend;

    for (const auto& part : m_parts) {
        const bool hasTex = (part.srv != nullptr);
        ID3D11BlendState* target = hasTex
            ? states->NonPremultiplied()
            : states->Opaque();

        if (bound != target) {
            ctx->OMSetBlendState(target, nullptr, 0xFFFFFFFF);
            bound = target;
        }

        fx->SetTextureEnabled(hasTex);
        if (hasTex) {
            fx->SetTexture(part.srv.Get());
        }

        fx->Apply(ctx);
        ctx->DrawIndexed(part.indexCount, part.startIndex, 0);
    }

    if (prevBlend) prevBlend->Release();
}

//==================================================================
// 骨: スケルトン構築（スキンクラスタから収集）
//==================================================================
bool UfbxStaticModel::BuildSkeletonFromScene(const ufbx_scene* scene)
{
    m_bones.clear();
    m_currWorld.clear();

    // cluster に登場するノードを骨として登録（最小集合）
    std::unordered_map<const ufbx_node*, int> indexOf;

    for (size_t ni = 0; ni < scene->nodes.count; ++ni) {
        const ufbx_node* node = scene->nodes.data[ni];
        const ufbx_mesh* mesh = node->mesh;
        if (!mesh) continue;

        for (size_t si = 0; si < mesh->skin_deformers.count; ++si) {
            const ufbx_skin_deformer* skin = mesh->skin_deformers.data[si];
            for (size_t ci = 0; ci < skin->clusters.count; ++ci) {
                const ufbx_skin_cluster* cl = skin->clusters.data[ci];
                if (!cl->bone_node) continue;

                int bi = AddBoneHelper(indexOf, m_bones, cl->bone_node); 

                // ボーン側バインド姿勢（bind_to_world）
                m_bones[bi].bindWorld = ToXMMatrix(cl->bind_to_world);
                {
                    XMMATRIX B = XMLoadFloat4x4(&m_bones[bi].bindWorld);
                    XMMATRIX B_inv = XMMatrixInverse(nullptr, B);
                    XMStoreFloat4x4(&m_bones[bi].invBindWorld, B_inv);
                }

                // ジオメトリ → ボーン の変換（geometry_to_bone）
                m_bones[bi].geomBindWorld = ToXMMatrix(cl->geometry_to_bone);
                {
                    XMMATRIX G = XMLoadFloat4x4(&m_bones[bi].geomBindWorld);
                    XMMATRIX G_inv = XMMatrixInverse(nullptr, G);
                    XMStoreFloat4x4(&m_bones[bi].invGeomBindWorld, G_inv);
                }

                // 親リンク（親が骨配列に存在するなら接続）
                if (const ufbx_node* p = cl->bone_node->parent) {
                    auto itp = indexOf.find(p);
                    if (itp != indexOf.end()) {
                        m_bones[bi].parent = itp->second;
                    }
                }
            }
        }
    }

    if (m_bones.empty()) return true;

    // 現在姿勢 = バインド姿勢で初期化
    m_currWorld.resize(m_bones.size());
    for (size_t i = 0; i < m_bones.size(); ++i) {
        m_currWorld[i] = m_bones[i].bindWorld;
    }

    // node → bone index マップ
    m_boneIndexOf_.clear();
    for (uint16_t i = 0; i < (uint16_t)m_bones.size(); ++i) {
        if (m_bones[i].node) {
            m_boneIndexOf_[m_bones[i].node] = i;
        }
    }

    return true;
}

//==================================================================
// 骨: アニメ評価（シーン全体の anim を使用する簡易版）
//==================================================================
void UfbxStaticModel::UpdateSkeletonAtTime(
    const ufbx_scene* scene,
    double            t_sec)
{
    const ufbx_anim* anim = scene ? scene->anim : nullptr;
    UpdateSkeletonAtTime(scene, anim, t_sec);
}

//==================================================================
// 骨: アニメ評価（特定の anim を使用）
//==================================================================
void UfbxStaticModel::UpdateSkeletonAtTime(
    const ufbx_scene* scene,
    const ufbx_anim* anim,
    double            t_sec)
{
    if (!scene || !anim || m_bones.empty()) return;

    // t をクランプ
    double t = t_sec;
    if (anim->time_end > anim->time_begin) {
        if (t < anim->time_begin) t = anim->time_begin;
        if (t > anim->time_end)   t = anim->time_end;
    }

    // node ごとの node_to_world(t) をキャッシュ
    std::unordered_map<const ufbx_node*, XMFLOAT4X4> cache;

    // 各ボーン：対応する node の node_to_world(t) を取得して m_currWorld に格納
    m_currWorld.resize(m_bones.size());
    for (size_t i = 0; i < m_bones.size(); ++i) {
        const ufbx_node* n = m_bones[i].node;
        if (!n) continue;
        m_currWorld[i] = EvaluateNodeWorldRecursive(n, anim, t, cache);
    }
}

//==================================================================
// 骨: アニメ評価（アニメスタック index 指定版）
//==================================================================
void UfbxStaticModel::UpdateSkeletonAtTime(
    const ufbx_scene* scene,
    size_t            stackIndex,
    double            t_sec)
{
    const ufbx_anim* anim = nullptr;
    if (scene && stackIndex < scene->anim_stacks.count) {
        const ufbx_anim_stack* st = scene->anim_stacks.data[stackIndex];
        anim = st ? st->anim : nullptr;
    }
    UpdateSkeletonAtTime(scene, anim, t_sec);
}

//==================================================================
// 骨: アニメ評価（アニメスタック名指定版）
//==================================================================
void UfbxStaticModel::UpdateSkeletonAtTime(
    const ufbx_scene* scene,
    std::string       stackName,
    double            t_sec)
{
    const ufbx_anim* anim = nullptr;
    if (scene && !stackName.empty()) {
        for (size_t i = 0; i < scene->anim_stacks.count; ++i) {
            const ufbx_anim_stack* st = scene->anim_stacks.data[i];
            if (st && st->name.data &&
                std::strcmp(st->name.data, stackName.c_str()) == 0) {
                anim = st->anim;
                break;
            }
        }
    }
    UpdateSkeletonAtTime(scene, anim, t_sec);
}

//==================================================================
// 骨: デバッグ描画（ライン）
//==================================================================
void UfbxStaticModel::DrawSkeleton(
    ID3D11DeviceContext* ctx,
    const XMMATRIX& world,
    const XMMATRIX& view,
    const XMMATRIX& proj)
{
    using namespace DirectX;

    if (!m_debugFx) {
        ID3D11Device* dev = nullptr;
        ctx->GetDevice(&dev);
        if (!dev) return;

        m_debugFx = std::make_unique<DXTK::BasicEffect>(dev);
        m_debugFx->SetVertexColorEnabled(true);
        m_debugFx->SetLightingEnabled(false);

        // 入力レイアウト: POSITION(float3) + COLOR(float4)
        const void* bc = nullptr;
        size_t      sz = 0;
        m_debugFx->GetVertexShaderBytecode(&bc, &sz);

        D3D11_INPUT_ELEMENT_DESC desc[] = {
            { "SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,
                0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",       0, DXGI_FORMAT_R32G32B32A32_FLOAT,
                0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        dev->CreateInputLayout(desc, 2, bc, sz, m_debugLayout.GetAddressOf());

        // 動的VBの初期確保
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(DebugVC) * 4096;
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        dev->CreateBuffer(&bd, nullptr, m_boneVB.GetAddressOf());
        m_boneVBSize = bd.ByteWidth / sizeof(DebugVC);

        dev->Release();
    }

    // ライン頂点を構築
    std::vector<DebugVC> lines;
    lines.reserve(m_bones.size() * 8);

    const XMFLOAT4 colBone = { 1.0f, 0.9f, 0.2f, 1.0f };
    const XMFLOAT4 colX = { 1,0,0,1 };
    const XMFLOAT4 colY = { 0,1,0,1 };
    const XMFLOAT4 colZ = { 0,0.5f,1,1 };

    // モデルスケールに応じた軸長（シーン半径の3%）
    const float axisLen = std::max(0.02f, m_sceneRadius * 0.03f);

    for (size_t i = 0; i < m_bones.size(); ++i) {
        const int         parent = m_bones[i].parent;
        const XMFLOAT4X4& Wi = m_currWorld[i];

        // 親→子ライン
        if (parent >= 0) {
            XMFLOAT3 p0 = GetPositionFromMatrix(m_currWorld[parent]);
            XMFLOAT3 p1 = GetPositionFromMatrix(Wi);
            lines.push_back({ p0, colBone });
            lines.push_back({ p1, colBone });
        }

        // 関節ローカル軸（RGBの短い3本）
        XMFLOAT3 o = GetPositionFromMatrix(Wi);
        XMFLOAT3 x1 = TransformPointByMatrix(Wi, { axisLen, 0, 0 });
        XMFLOAT3 y1 = TransformPointByMatrix(Wi, { 0, axisLen, 0 });
        XMFLOAT3 z1 = TransformPointByMatrix(Wi, { 0, 0, axisLen });

        lines.push_back({ o,  colX }); lines.push_back({ x1, colX });
        lines.push_back({ o,  colY }); lines.push_back({ y1, colY });
        lines.push_back({ o,  colZ }); lines.push_back({ z1, colZ });
    }

    if (lines.empty()) return;

    // 動的VBサイズが足りなければ拡張
    if (m_boneVBSize < lines.size()) {
        m_boneVB.Reset();

        ID3D11Device* dev = nullptr;
        ctx->GetDevice(&dev);
        if (!dev) return;

        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = (UINT)(lines.size() * sizeof(DebugVC));
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        dev->CreateBuffer(&bd, nullptr, m_boneVB.GetAddressOf());
        m_boneVBSize = lines.size();

        dev->Release();
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(ctx->Map(m_boneVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        std::memcpy(mapped.pData, lines.data(), lines.size() * sizeof(DebugVC));
        ctx->Unmap(m_boneVB.Get(), 0);
    }

    UINT          stride = sizeof(DebugVC);
    UINT          offset = 0;
    ID3D11Buffer* vb = m_boneVB.Get();

    ctx->IASetInputLayout(m_debugLayout.Get());
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    m_debugFx->SetWorld(world);
    m_debugFx->SetView(view);
    m_debugFx->SetProjection(proj);
    m_debugFx->Apply(ctx);

    ctx->Draw((UINT)lines.size(), 0);
}
