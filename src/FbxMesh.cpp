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
// ビルド用コンテキスト
//------------------------------------------------------------
struct BuildContext
{
    const ufbx_mesh* mesh = nullptr;
    const ufbx_vertex_vec2* uvv = nullptr;
    const std::vector<FbxMesh::VertexInfluence>* infl_per_vtx = nullptr;

};

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
    // スキン情報を構築（1 メッシュ単位）
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

        // 一旦「ボーン番号＋ウェイト」のリストをためてから 4 本に絞る
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

        // 4 本までに絞って正規化
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
    // マテリアル／テクスチャに対応した UV セットを選ぶ
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

//================================================================
// FbxMesh::EmitCorner（1 コーナーから VertexPNT2 を組み立てて push）
//================================================================
void FbxMesh::EmitCorner(
    BuildContext& ctx,
    uint32_t corner,
    uint32_t vtx)
{
    using namespace DirectX;

    const ufbx_mesh* mesh = ctx.mesh;

    // 位置
    uint32_t pi = UfbxUtil::ValueIndexOf(mesh->vertex_position, corner, vtx);
    ufbx_vec3 p = mesh->vertex_position.values.data[pi];
    XMFLOAT3 P((float)p.x, (float)p.y, (float)p.z);

    // AABB 更新
    bounds_.WrapBox(P);

    // 法線
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
    }

    // UV（V だけ反転）
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

    // 展開時に使ったフラット頂点は破棄（バインド／スキン頂点は残す）
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
    bounds_.Reset();

    // Skeleton 内のボーンマップ
    const auto& bone_index_map = skeleton.Data().bone_index_of_;


    has_skinning_ = false;
    // シーン中の全ノードをチェック
    for (size_t ni = 0; ni < scene->nodes.count; ++ni) {
        const ufbx_node* node = scene->nodes.data[ni];
        const ufbx_mesh* mesh = node->mesh;
        if (!mesh) continue;

        if (mesh->skin_deformers.count > 0) {
            has_skinning_ = true;   // ★ ここで設定（最初の1回でOK）
        }

        // 頂点ごとのスキン情報
        std::vector<VertexInfluence> infl_per_vtx;
        BuildInfluencesForMesh(mesh, bone_index_map, infl_per_vtx);

        // 基本 UV セット（とりあえず 1 つ）
        const ufbx_vertex_vec2* base_uv = nullptr;
        if (mesh->vertex_uv.exists) {
            base_uv = &mesh->vertex_uv;
        }
        else if (mesh->uv_sets.count > 0 &&
            mesh->uv_sets.data[0].vertex_uv.exists)
        {
            base_uv = &mesh->uv_sets.data[0].vertex_uv;
        }

        // フェイスをマテリアルごとにグループ分け
        std::unordered_map<uint32_t, std::vector<uint32_t>> faces_by_mat;
        for (uint32_t fi = 0; fi < (uint32_t)mesh->faces.count; ++fi) {
            uint32_t mi =
                (mesh->face_material.count > 0) ?
                mesh->face_material.data[fi] : 0;
            faces_by_mat[mi].push_back(fi);
        }

        // 各マテリアルごとに MeshPart を作って頂点展開
        for (auto& kv : faces_by_mat) {
            uint32_t                     mat_index = kv.first;
            const std::vector<uint32_t>& face_list = kv.second;

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
                else if (mesh && mesh->materials.count > mat_index) {
                    mat = mesh->materials.data[mat_index];
                }
                part.mat = mat;
            }

            // UV セットをテクスチャに合わせて選び直す
            const ufbx_vertex_vec2* uvv =
                ChooseUVSet(mesh, part.mat, base_uv);

            // このマテリアルで使うコンテキストを準備
            BuildContext ctx;
            ctx.mesh = mesh;
            ctx.uvv = uvv;
            ctx.infl_per_vtx = &infl_per_vtx;

            // 登録されたフェイスを全て三角形に分解して頂点生成
            for (uint32_t f_index : face_list) {
                const ufbx_face f = mesh->faces.data[f_index];
                if (f.num_indices < 3) continue;

                // n角形を「扇形分割」で三角形にする
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

            // この MeshPart が使うインデックス数を記録
            part.index_count =
                (uint32_t)mesh_.indices_.size() - part.start_index;
            if (part.index_count > 0) {
                mesh_.parts_.push_back(part);
            }
        }
    }

    // ★AABB から球を更新
    bounds_.RecalcSphereFromAABB();

    // スケルトン側にシーン半径を書き込む（ボーン表示で使用）
    skeleton.Data().scene_radius_ = bounds_.radius;

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
    const ufbx_scene* /*scene*/)
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

// FbxMesh.cpp

void FbxMesh::ApplySkinCPU(
    const std::vector<DirectX::XMMATRIX>& skin_mats)
{
    using namespace DirectX;

    // 参照のショートカット
    const auto& influences = mesh_.influences_;
    const auto& bind_vertices = mesh_.bind_vertices_;
    auto& out_vertices = mesh_.skinned_vertices_;

    size_t n = bind_vertices.size();

    if (n == 0) {
        return;
    }

    // サイズがズレていたら skinned を合わせておく
    if (out_vertices.size() != n) {
        out_vertices.resize(n);
    }

    // インフルエンスの数が足りない場合は安全側に寄せる
    if (influences.size() < n) {
        n = influences.size();
    }

    for (size_t v = 0; v < n; ++v) {
        const VertexInfluence& inf = influences[v];

        XMVECTOR P = XMVectorZero(); // 合成された位置
        XMVECTOR N = XMVectorZero(); // 合成された法線
        bool     any = false;          // 1つでも有効なボーンがあったか

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
        VertexPNT2 sv = bind_vertices[v];
        XMStoreFloat3(&sv.pos, P);
        N = XMVector3Normalize(N);
        XMStoreFloat3(&sv.nrm, N);
        out_vertices[v] = sv;
    }
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
    if (has_skinning_) {
        // CPU スキニング（ボーン情報とウェイトがある場合のみ）
        if (!mesh_.influences_.empty() && !mesh_.bind_vertices_.empty()) {
            auto& skin_mats = skeleton.SkinMatrices();
            //skin_mats.resize(skeleton.Bones().size()); //いるのか要らねぇのかわからない

            for (size_t i = 0; i < skeleton.Bones().size(); ++i) {
                XMMATRIX W =
                    XMLoadFloat4x4(&skeleton.CurrWorld()[i]);
                XMMATRIX G2B =
                    XMLoadFloat4x4(&skeleton.Bones()[i].geom_bind_world);
                // スキン行列 = 現在ボーン姿勢 × ジオメトリ→ボーン
                skin_mats[i] = XMMatrixMultiply(G2B, W);
            }

            // CPU でスキニングして頂点を更新
            ApplySkinCPU(skin_mats);

            // GPU の頂点バッファにスキニング結果を書き戻す
            ctx->UpdateSubresource(
                vb_.Get(), 0, nullptr,
                &mesh_.skinned_vertices_[0], 0, 0);

            // CPU スキニングが終わった直後あたりに追加（デバッグ用）
            {
                float minY = FLT_MAX;
                float maxY = -FLT_MAX;

                for (const auto& v : mesh_.skinned_vertices_) {
                    if (v.pos.y < minY) minY = v.pos.y;
                    if (v.pos.y > maxY) maxY = v.pos.y;
                }

                float skinnedHeight = (maxY - minY);

                //char buf[256];
                //std::snprintf(
                //    buf, sizeof(buf),
                //    "[SkinnedAABB] skinnedHeight=%.6f\n",
                //    skinnedHeight
                //);
                //OutputDebugStringA(buf);
            }
        }
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

void FbxMesh::ApplyUniformScale(float s)
{
    if (s <= 0.0f) return;

    // --------------------------------------------------------
    // フラット頂点（GPUアップロード前の元データ）
    // --------------------------------------------------------
    for (auto& v : mesh_.vertices_) {
        v.pos.x *= s;
        v.pos.y *= s;
        v.pos.z *= s;
    }

    // --------------------------------------------------------
    // バインドポーズ頂点
    // --------------------------------------------------------
    for (auto& v : mesh_.bind_vertices_) {
        v.pos.x *= s;
        v.pos.y *= s;
        v.pos.z *= s;
    }

    // --------------------------------------------------------
    // スキニング後頂点（初期状態）
    // --------------------------------------------------------
    for (auto& v : mesh_.skinned_vertices_) {
        v.pos.x *= s;
        v.pos.y *= s;
        v.pos.z *= s;
    }

    // --------------------------------------------------------
    // バウンディングボリューム
    // --------------------------------------------------------
    bounds_.Scale(s);
}

