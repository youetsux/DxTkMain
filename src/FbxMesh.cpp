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
#include <Effects.h>        // ★追加: BasicEffect の実体
#include <CommonStates.h>   // ★追加: CommonStates の実体

#include "Gfx.h"
#include "ufbx.h"
#include "UfbxUtil.h"

using Microsoft::WRL::ComPtr;

namespace
{
    using namespace DirectX;
    namespace fs = std::filesystem;

    // ------------------------------------------------------------
    // ボーンウェイトのソート用
    // ------------------------------------------------------------
    bool CompareBoneWeightPair(
        const std::pair<uint16_t, float>& a,
        const std::pair<uint16_t, float>& b)
    {
        return a.second > b.second;
    }

    // ------------------------------------------------------------
    // CPU スキニング
    // ------------------------------------------------------------
    void ApplySkinCPU(
        const std::vector<XMMATRIX>& skin_mats,
        const std::vector<FbxMesh::VertexInfluence>& influences,
        const std::vector<FbxMesh::VertexPNT2>& bind_vertices,
        std::vector<FbxMesh::VertexPNT2>& out_vertices)
    {
        size_t n = bind_vertices.size();
        //out_vertices.resize(n);

        for (size_t v = 0; v < n; ++v) {
            const FbxMesh::VertexInfluence& inf = influences[v];

            XMVECTOR P = XMVectorZero(); // 合成された位置
            XMVECTOR N = XMVectorZero(); // 合成された法線
            bool any = false;            // 1つでも有効なボーンがあったか

            for (int k = 0; k < 4; ++k) {
                float    w = inf.weight[k];
                uint16_t b = inf.bone[k];

                if (w <= 0.0f)             continue; // ウェイト0は無視
                if (b >= skin_mats.size()) continue; // 不正なボーン番号も無視

                const XMMATRIX& B = skin_mats[b];
                XMVECTOR W = XMVectorReplicate(w);   // w → {w,w,w,w}

                // P += (B * pos) * w
                P = XMVectorMultiplyAdd(
                    UfbxUtil::TransformPosition(bind_vertices[v].pos, B),
                    W, P);

                // N += (B * nrm) * w
                N = XMVectorMultiplyAdd(
                    UfbxUtil::TransformNormal(bind_vertices[v].nrm, B),
                    W, N);

                any = true;
            }

            // どのボーンからも影響がなければ、元の頂点を使う
            if (!any) {
                out_vertices[v] = bind_vertices[v];
                continue;
            }

            // 合成結果を構造体に書き戻す
            FbxMesh::VertexPNT2 sv = bind_vertices[v];
            XMStoreFloat3(&sv.pos, P);
            N = XMVector3Normalize(N);
            XMStoreFloat3(&sv.nrm, N);
            out_vertices[v] = sv;
        }
    }
} // anonymous namespace

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

    mesh_.vertices_.clear();
    mesh_.vertices_.shrink_to_fit();

    return true;
}

//================================================================
// メッシュ展開（CPU）
//================================================================
void FbxMesh::ExpandAllNodes(const ufbx_scene* scene,
    FbxSkeleton& skeleton)
{
    using namespace DirectX;

    // いったん全部クリア
    mesh_.vertices_.clear();
    mesh_.indices_.clear();
    mesh_.parts_.clear();
    mesh_.influences_.clear();
    mesh_.bind_vertices_.clear();
    mesh_.skinned_vertices_.clear();

    // バウンディングボックス初期化
    XMFLOAT3 bb_min;
    XMFLOAT3 bb_max;
    bb_min.x = FLT_MAX;  bb_min.y = FLT_MAX;  bb_min.z = FLT_MAX;
    bb_max.x = -FLT_MAX; bb_max.y = -FLT_MAX; bb_max.z = -FLT_MAX;

    // ★ここを「今まで動いていた形」に合わせておく
    // 以前 UfbxStaticModel_Mesh.cpp で使っていた:
    //   skeleton_.Data().bone_index_of_
    const auto& bone_index_map = skeleton.Data().bone_index_of_;

    // シーン中の全ノードをチェック
    for (size_t ni = 0; ni < scene->nodes.count; ++ni) {
        const ufbx_node* node = scene->nodes.data[ni];
        const ufbx_mesh* mesh = node->mesh;
        if (!mesh) continue;

        // 頂点ごとのスキン情報（BoneIndex + Weight）を格納
        std::vector<VertexInfluence> infl_per_vtx;

        if (mesh->skin_deformers.count > 0) {
            infl_per_vtx.assign(
                mesh->num_vertices,
                VertexInfluence());

            const ufbx_skin_deformer* skin =
                mesh->skin_deformers.data[0];

            const ufbx_skin_cluster_list& clusters = skin->clusters;
            const ufbx_skin_vertex_list& vtx_list = skin->vertices;
            const ufbx_skin_weight_list& w_list = skin->weights;

            // 一旦「ボーン番号＋ウェイト」のリストをためてから 4 本に絞る
            std::vector< std::vector< std::pair<uint16_t, float> > >
                acc(mesh->num_vertices);

            for (size_t v = 0; v < vtx_list.count; ++v) {
                const ufbx_skin_vertex sv = vtx_list.data[v];
                uint32_t begin = sv.weight_begin;
                uint32_t n = sv.num_weights;

                for (uint32_t k = 0; k < n; ++k) {
                    const ufbx_skin_weight w = w_list.data[begin + k];
                    uint32_t ci = w.cluster_index;
                    if (ci >= clusters.count) continue;

                    const ufbx_skin_cluster* cl =
                        clusters.data[ci];
                    if (!cl || !cl->bone_node) continue;

                    // ★ Skeleton 内にあるボーンインデックスマップを参照
                    auto it = bone_index_map.find(cl->bone_node);
                    if (it == bone_index_map.end()) {
                        continue;
                    }

                    acc[v].push_back(
                        std::make_pair(it->second, (float)w.weight));
                }
            }

            // 4 本までに絞って正規化
            for (size_t v2 = 0; v2 < acc.size(); ++v2) {
                std::vector< std::pair<uint16_t, float> >& list = acc[v2];
                if (list.empty()) continue;

                // ウェイトの大きい順にソート
                std::sort(list.begin(), list.end(), CompareBoneWeightPair);

                // 最大 4 本だけ使う
                if (list.size() > 4) {
                    list.resize(4);
                }

                float sum = 0.0f;
                for (size_t i = 0; i < list.size(); ++i) {
                    sum += list[i].second;
                }
                if (sum <= 0.0f) sum = 1.0f;

                VertexInfluence vi;
                size_t n_infl = list.size();
                if (n_infl > 4) n_infl = 4;

                for (size_t i = 0; i < n_infl; ++i) {
                    vi.bone[i] = list[i].first;
                    vi.weight[i] = list[i].second / sum; // 合計 ≒ 1 に正規化
                }
                infl_per_vtx[v2] = vi;
            }
        }

        // 基本 UV セット（とりあえず 1 つ選ぶ）
        const ufbx_vertex_vec2* base_uv = nullptr;
        if (mesh->vertex_uv.exists) {
            base_uv = &mesh->vertex_uv;
        }
        else if (mesh->uv_sets.count > 0 &&
            mesh->uv_sets.data[0].vertex_uv.exists)
        {
            base_uv = &mesh->uv_sets.data[0].vertex_uv;
        }

        // フェイスを「マテリアルごと」にグループ分け
        std::unordered_map<uint32_t, std::vector<uint32_t> > faces_by_mat;

        for (uint32_t fi = 0; fi < (uint32_t)mesh->faces.count; ++fi) {
            uint32_t mi =
                (mesh->face_material.count > 0) ?
                mesh->face_material.data[fi] : 0;
            faces_by_mat[mi].push_back(fi);
        }

        // 各マテリアルごとに MeshPart を作って頂点展開
        auto it_mat = faces_by_mat.begin();
        for (; it_mat != faces_by_mat.end(); ++it_mat) {
            uint32_t                     mat_index = it_mat->first;
            const std::vector<uint32_t>& face_list = it_mat->second;

            MeshPart part;
            part.mat = nullptr;
            part.start_index = (uint32_t)mesh_.indices_.size();

            // 正しいマテリアルを node / mesh から探す
            {
                const ufbx_material* mat = nullptr;
                if (node && node->materials.count > mat_index &&
                    node->materials.data[mat_index])
                {
                    mat = node->materials.data[mat_index];
                }
                else if (mesh && mesh->materials.count > mat_index)
                {
                    mat = mesh->materials.data[mat_index];
                }
                part.mat = mat;
            }

            // UV セットをテクスチャに合わせて選び直す
            const ufbx_texture* tex_for_uv = UfbxUtil::GetDiffuseTexture(part.mat);
            const ufbx_vertex_vec2* uvv = base_uv;
            if (tex_for_uv && tex_for_uv->uv_set.length > 0) {
                const ufbx_vertex_vec2* alt =
                    UfbxUtil::ResolveUVByName(mesh, tex_for_uv->uv_set);
                if (alt && alt->exists) {
                    uvv = alt;
                }
            }

            // 登録されたフェイスを全て三角形に分解して頂点生成
            for (size_t face_idx = 0; face_idx < face_list.size(); ++face_idx) {
                uint32_t      f_index = face_list[face_idx];
                const ufbx_face f = mesh->faces.data[f_index];
                if (f.num_indices < 3) continue;

                // n角形を「扇形分割」で三角形にする
                for (uint32_t k = 0; k + 2 < f.num_indices; ++k) {
                    uint32_t corners[3];
                    corners[0] = f.index_begin + 0;
                    corners[1] = f.index_begin + (k + 1);
                    corners[2] = f.index_begin + (k + 2);

                    for (int c = 0; c < 3; ++c) {
                        uint32_t corner = corners[c];
                        uint32_t vtx = mesh->vertex_indices.data[corner];

                        // 位置
                        uint32_t pi =
                            UfbxUtil::ValueIndexOf(mesh->vertex_position, corner, vtx);
                        ufbx_vec3 p =
                            mesh->vertex_position.values.data[pi];
                        XMFLOAT3 P;
                        P.x = (float)p.x;
                        P.y = (float)p.y;
                        P.z = (float)p.z;

                        // AABB（バウンディングボックス）更新
                        if (P.x < bb_min.x) bb_min.x = P.x;
                        if (P.y < bb_min.y) bb_min.y = P.y;
                        if (P.z < bb_min.z) bb_min.z = P.z;
                        if (P.x > bb_max.x) bb_max.x = P.x;
                        if (P.y > bb_max.y) bb_max.y = P.y;
                        if (P.z > bb_max.z) bb_max.z = P.z;

                        // 法線
                        XMFLOAT3 N;
                        N.x = 0.0f; N.y = 1.0f; N.z = 0.0f;
                        if (mesh->vertex_normal.exists) {
                            uint32_t ni2 =
                                UfbxUtil::ValueIndexOf(mesh->vertex_normal, corner, vtx);
                            ufbx_vec3 n =
                                mesh->vertex_normal.values.data[ni2];

                            double len = std::sqrt(
                                n.x * n.x + n.y * n.y + n.z * n.z);
                            if (len > 0.0) {
                                double inv = 1.0 / len;
                                N.x = (float)(n.x * inv);
                                N.y = (float)(n.y * inv);
                                N.z = (float)(n.z * inv);
                            }
                        }

                        // UV (V だけ反転: 1 - v)
                        XMFLOAT2 T;
                        T.x = 0.0f; T.y = 0.0f;
                        if (uvv && uvv->exists) {
                            uint32_t ti =
                                UfbxUtil::ValueIndexOf(*uvv, corner, vtx);
                            ufbx_vec2 t =
                                uvv->values.data[ti];
                            T.x = (float)t.x;
                            T.y = (float)(1.0 - t.y);
                        }

                        // 頂点構造体を埋める
                        VertexPNT2 vtx_out;
                        vtx_out.pos = P;
                        vtx_out.nrm = N;
                        vtx_out.uv = T;

                        // スキン情報（ボーンとウェイト）
                        VertexInfluence vi;
                        if (!infl_per_vtx.empty()) {
                            vi = infl_per_vtx[vtx];
                        }

                        // 自前の配列に追加
                        mesh_.influences_.push_back(vi);
                        mesh_.indices_.push_back(
                            (uint32_t)mesh_.vertices_.size());
                        mesh_.vertices_.push_back(vtx_out);
                    }
                }
            }

            // この MeshPart が使うインデックス数を記録
            part.index_count =
                (uint32_t)mesh_.indices_.size() - part.start_index;
            if (part.index_count > 0) {
                mesh_.parts_.push_back(part);
            }
        }
    }

    // シーンの「おおよその半径」を求める（デバッグ用）
    XMFLOAT3 ext;
    ext.x = (bb_max.x - bb_min.x) * 0.5f;
    ext.y = (bb_max.y - bb_min.y) * 0.5f;
    ext.z = (bb_max.z - bb_min.z) * 0.5f;

    float a = (float)std::fabs(ext.x);
    float b = (float)std::fabs(ext.y);
    float c = (float)std::fabs(ext.z);

    float r = a;
    if (b > r) r = b;
    if (c > r) r = c;
    if (r < 1e-3f) r = 1.0f;

    // スケルトン側にシーン半径を書き込む（ボーン表示で使用）
    skeleton.Data().scene_radius_ = r;

    // バインドポーズ頂点・スキニング結果頂点を準備
    mesh_.bind_vertices_ = mesh_.vertices_;
    mesh_.skinned_vertices_ = mesh_.vertices_;
}

//================================================================
// GPU バッファ作成
//================================================================
bool FbxMesh::CreateGpuBuffers()
{
    if (mesh_.vertices_.empty()) return false;

    ID3D11Device* device = Gfx::Dev();
    if (!device) return false;

    // VB 作成
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

    // IB 作成
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
// エフェクト＆テクスチャ作成
//================================================================
bool FbxMesh::CreateEffectsAndTextures(
    const char* fbx_path,
    const ufbx_scene* scene)
{
    using namespace DirectX;
    namespace fs = std::filesystem;

    ID3D11Device* device = Gfx::Dev();
    ID3D11DeviceContext* ctx = Gfx::Ctx();
    if (!device || !ctx) return false;

    // CommonStates / BasicEffect の確保
    if (!states_) {
        states_.reset(new DirectX::DX11::CommonStates(device));
    }
    if (!fx_) {
        fx_.reset(new DirectX::DX11::BasicEffect(device));
    }

    DirectX::BasicEffect* fx = fx_.get();

    // 簡単なライティング設定
    fx->SetLightingEnabled(true);
    fx->SetPerPixelLighting(true);
    fx->SetVertexColorEnabled(false);
    fx->SetTextureEnabled(true);
    fx->SetAmbientLightColor({ 0.3f, 0.3f, 0.3f });
    fx->SetDiffuseColor({ 1.0f,  1.0f,  1.0f, 1.0f });
    fx->SetLightEnabled(0, true);
    fx->SetLightDirection(0, { -0.5f, -1.0f, 0.3f });
    fx->SetLightDiffuseColor(0, { 1.0f, 1.0f, 1.0f, 1.0f });

    // 入力レイアウト（頂点の並び方の説明）
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

    // テクスチャロード用に FBX ファイルのディレクトリを覚えておく
    fs::path fbx_dir;
    if (fbx_path) {
        size_t len = std::strlen(fbx_path);
        fbx_dir = UfbxUtil::PathFromUtf8(fbx_path, len).parent_path();
    }

    // 各 MeshPart のテクスチャを作成
    for (size_t i = 0; i < mesh_.parts_.size(); ++i) {
        MeshPart& part = mesh_.parts_[i];

        const ufbx_texture* tex = UfbxUtil::GetDiffuseTexture(part.mat);
        if (!tex) continue;

        HRESULT hr = E_FAIL;

        // FBX 内にテクスチャの生データが埋め込まれている場合
        if (tex->content.size > 0 && tex->content.data) {
            hr = DirectX::CreateWICTextureFromMemory(
                device,
                ctx,
                reinterpret_cast<const uint8_t*>(tex->content.data),
                tex->content.size,
                nullptr,
                part.srv.ReleaseAndGetAddressOf());
        }
        // ファイル名だけ（外部ファイル）の場合
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

        // 読み込みに失敗したら SRV を空にしておく
        if (FAILED(hr)) {
            part.srv.Reset();
        }
    }

    return true;
}

//================================================================
// メッシュ描画
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

    // CPU スキニング（ボーン情報とウェイトがある場合のみ）
    if (!mesh_.influences_.empty() && !mesh_.bind_vertices_.empty()) {
        auto& skin_mats = skeleton.SkinMatrices();
       //skin_mats.resize(skeleton.Bones().size());

        for (size_t i = 0; i < skeleton.Bones().size(); ++i) {
            XMMATRIX W =
                XMLoadFloat4x4(&skeleton.CurrWorld()[i]);
            XMMATRIX G2B =
                XMLoadFloat4x4(&skeleton.Bones()[i].geom_bind_world);
            // スキン行列 = 現在ボーン姿勢 × ジオメトリ→ボーン
            skin_mats[i] = XMMatrixMultiply(G2B, W);
        }

        // CPU でスキニングして頂点を更新
        ApplySkinCPU(
            skin_mats,
            mesh_.influences_,
            mesh_.bind_vertices_,
            mesh_.skinned_vertices_);

        // GPU の頂点バッファにスキニング結果を書き戻す
        ctx->UpdateSubresource(
            vb_.Get(), 0, nullptr,
            &mesh_.skinned_vertices_[0], 0, 0);
    }

    // IA ステージ設定
    UINT stride = sizeof(VertexPNT2);
    UINT offset = 0;
    ID3D11Buffer* vb = vb_.Get();

    ctx->IASetInputLayout(layout_.Get());
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(ib_.Get(), DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 深度ステンシル・ラスタライザ・サンプラ設定
    ctx->OMSetDepthStencilState(states_->DepthDefault(), 0);
    ctx->RSSetState(states_->CullCounterClockwise());

    ID3D11SamplerState* samp = states_->LinearClamp();
    ctx->PSSetSamplers(0, 1, &samp);

    // 行列設定
    fx_->SetWorld(world);
    fx_->SetView(view);
    fx_->SetProjection(proj);

    // 現在のブレンドステートを退避
    ID3D11BlendState* prev_blend = nullptr;
    FLOAT             prev_factor[4];
    UINT              prev_mask = 0xFFFFFFFF;
    ctx->OMGetBlendState(&prev_blend, prev_factor, &prev_mask);

    ID3D11BlendState* bound = prev_blend;

    // 各 MeshPart を描画
    for (size_t i_part = 0; i_part < mesh_.parts_.size(); ++i_part) {
        const MeshPart& part = mesh_.parts_[i_part];

        bool has_tex = (part.srv != nullptr);
        // テクスチャありなら αブレンド（NonPremultiplied）、なければ不透明
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

    // ブレンドステートを元に戻す
    if (prev_blend) {
        ctx->OMSetBlendState(prev_blend, prev_factor, prev_mask);
        prev_blend->Release();
    }
}
