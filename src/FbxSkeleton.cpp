#include "FbxSkeleton.h"
#include "ufbx.h"
#include "UfbxUtil.h"
#include "Gfx.h"        // DrawDebug 実装時に使う想定（今は未使用）
#include <Effects.h>

#include <unordered_map>

//------------------------------------------------------------
// 内部ヘルパ（匿名名前空間）
//------------------------------------------------------------
namespace
{
    // ボーン登録ヘルパ
    // ・ufbx_node を SkeletonData に追加し、そのインデックスを返す
    // ・すでに登録されている場合は既存のインデックスを返す
    int AddBoneInternal(
        const ufbx_node* node,
        SkeletonData& skeleton,
        std::unordered_map<const ufbx_node*, int>& index_of)
    {
        // すでに登録済みかチェック
        auto it = index_of.find(node);
        if (it != index_of.end()) {
            return it->second;
        }

        // 新しいボーンとして追加
        int idx = static_cast<int>(skeleton.bones_.size());
        index_of.insert(std::make_pair(node, idx));

        BoneInfo info;
        info.node = node;
        skeleton.bones_.push_back(info);
        return idx;
    }


    // 骨デバッグ描画
    std::vector<BoneInfo>        m_bones;     // 骨の基本情報
    std::vector<DirectX::XMFLOAT4X4> m_currWorld; // W_i(t)

    std::unique_ptr<DirectX::DX11::BasicEffect> m_debugFx;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>   m_debugLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer>        m_boneVB;
    size_t m_boneVBSize = 0;

    // 既存の AddBoneInternal や m_debugFx, m_boneVBSize などに続けて…
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> g_skelDepthState; // Z test on / Z write off
}


const std::vector<BoneInfo>& FbxSkeleton::Bones() const
{
    return data_.bones_;
}

const std::vector<DirectX::XMFLOAT4X4>& FbxSkeleton::CurrWorld() const
{
    return data_.curr_world_;
}

float FbxSkeleton::SceneRadius() const
{
    return data_.scene_radius_;
}

const std::unordered_map<const ufbx_node*, uint16_t>& FbxSkeleton::BoneIndexMap() const
{
    return data_.bone_index_of_;
}

const std::vector<DirectX::XMMATRIX>& FbxSkeleton::SkinMatrices() const
{
    return data_.skin_mats_;
}

std::vector<DirectX::XMMATRIX>& FbxSkeleton::SkinMatrices()
{
    return data_.skin_mats_;
}

bool FbxSkeleton::BuildFromScene(const ufbx_scene* scene)
{
    // いったん全部クリア
    data_.bones_.clear();
    data_.curr_world_.clear();
    data_.bone_index_of_.clear();
    data_.skin_mats_.clear();
    data_.scene_radius_ = 1.0f;

    if (!scene) {
        return false;
    }

    // ufbx_node* → ボーンインデックス の一時マップ
    std::unordered_map<const ufbx_node*, int> index_of;

    // 全ノードをループ
    for (size_t ni = 0; ni < scene->nodes.count; ++ni) {
        const ufbx_node* node = scene->nodes.data[ni];
        const ufbx_mesh* mesh = node->mesh;
        if (!mesh) continue; // メッシュを持たないノードはスキップ

        // メッシュについているスキンデフォーマを全て見る
        for (size_t si = 0; si < mesh->skin_deformers.count; ++si) {
            const ufbx_skin_deformer* skin = mesh->skin_deformers.data[si];

            // 各クラスター（ボーン＋頂点群）の情報を見る
            for (size_t ci = 0; ci < skin->clusters.count; ++ci) {
                const ufbx_skin_cluster* cl = skin->clusters.data[ci];
                if (!cl->bone_node) continue;

                // このクラスターのボーンを SkeletonData に登録
                int bi = AddBoneInternal(
                    cl->bone_node, data_, index_of);

                // ボーン側のバインド姿勢（ボーンのワールド行列）
                data_.bones_[bi].bind_world =
                    UfbxUtil::ToXMMatrix(cl->bind_to_world);

                // バインド姿勢の逆行列も作っておく
                {
                    DirectX::XMMATRIX B =
                        DirectX::XMLoadFloat4x4(&data_.bones_[bi].bind_world);
                    DirectX::XMMATRIX B_inv = DirectX::XMMatrixInverse(nullptr, B);
                    DirectX::XMStoreFloat4x4(
                        &data_.bones_[bi].inv_bind_world, B_inv);
                }

                // ジオメトリ → ボーン の変換行列
                data_.bones_[bi].geom_bind_world =
                    UfbxUtil::ToXMMatrix(cl->geometry_to_bone);

                // その逆行列
                {
                    DirectX::XMMATRIX G =
                        DirectX::XMLoadFloat4x4(&data_.bones_[bi].geom_bind_world);
                    DirectX::XMMATRIX G_inv = DirectX::XMMatrixInverse(nullptr, G);
                    DirectX::XMStoreFloat4x4(
                        &data_.bones_[bi].inv_geom_bind_world, G_inv);
                }

                // 親ボーンとの関係（親があればインデックスを調べてセット）
                if (cl->bone_node->parent) {
                    auto itp = index_of.find(cl->bone_node->parent);
                    if (itp != index_of.end()) {
                        data_.bones_[bi].parent = itp->second;
                    }
                }
            }
        }
    }

    // ボーンが 1 本も無ければ、そのまま true（メッシュだけのモデル対応）
    if (data_.bones_.empty()) {
        return true;
    }

    // 現在姿勢・スキン行列用配列をボーン数に合わせて確保
    data_.curr_world_.resize(data_.bones_.size());
    data_.skin_mats_.resize(data_.bones_.size());

    // ufbx_node* → uint16_t ボーン番号 のマップを作る
    for (size_t i = 0; i < data_.bones_.size(); ++i) {
        // 初期姿勢はバインド姿勢
        data_.curr_world_[i] = data_.bones_[i].bind_world;

        if (data_.bones_[i].node && i < 0x10000) {
            uint16_t i16 = static_cast<uint16_t>(i);
            data_.bone_index_of_[data_.bones_[i].node] = i16;
        }
    }

    return true;
}

void FbxSkeleton::UpdateAtTime(const ufbx_scene* scene, const ufbx_anim* anim, double t_sec)
{
    if (!scene || !anim) return;
    if (data_.bones_.empty()) return;

    // t をアニメーション時間の範囲にクランプ
    double t = t_sec;
    if (anim->time_end > anim->time_begin) {
        if (t < anim->time_begin) t = anim->time_begin;
        if (t > anim->time_end)   t = anim->time_end;
    }

    // 計算済みノードの結果をためるキャッシュ
    static std::unordered_map<const ufbx_node*, DirectX::XMFLOAT4X4> cache;
    cache.clear();
    data_.curr_world_.resize(data_.bones_.size());

    for (size_t i = 0; i < data_.bones_.size(); ++i) {
        const ufbx_node* node = data_.bones_[i].node;
        if (!node) continue;

        // ボーンに対応するノードのワールド行列を求める
        data_.curr_world_[i] =
            UfbxUtil::EvaluateNodeWorldRecursive(node, anim, t, cache);
    }
}
namespace {
    // ------------------------------------------------------------
    // 骨の位置/変換ヘルパ（DrawSkeleton で使う）
    // ------------------------------------------------------------
    DirectX::XMFLOAT3 GetBonePosition(const DirectX::XMFLOAT4X4& M)
    {
        // 行列の第4行の xyz が平行移動成分
        DirectX::XMFLOAT3 p;
        p.x = M._41;
        p.y = M._42;
        p.z = M._43;
        return p;
    }
    // 任意の点 v を行列 M で変換
    DirectX::XMFLOAT3 TransformPoint(
        const DirectX::XMFLOAT4X4& M,
        const DirectX::XMFLOAT3& v)
    {
        using namespace DirectX;

        XMMATRIX mat = XMLoadFloat4x4(&M);
        XMVECTOR p = XMVector3Transform(XMLoadFloat3(&v), mat);
        XMFLOAT3 out;
        XMStoreFloat3(&out, p);
        return out;
    }

}


void FbxSkeleton::DrawDebug(
    const DirectX::XMMATRIX& world,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& proj)
{
    using namespace DirectX;

    struct DebugVC
    {
        XMFLOAT3 pos;
        XMFLOAT4 col;
    };

    ID3D11DeviceContext* ctx = Gfx::Ctx();
    if (!ctx) return;
    if (data_.bones_.empty()) return;

    // ---------- BasicEffect / VB 初期化 ----------
    if (!m_debugFx)
    {
        ID3D11Device* dev = nullptr;
        ctx->GetDevice(&dev);
        if (!dev) return;

        m_debugFx = std::make_unique<BasicEffect>(dev);
        m_debugFx->SetVertexColorEnabled(true);
        m_debugFx->SetLightingEnabled(false);

        const void* bc = nullptr;
        size_t      sz = 0;
        m_debugFx->GetVertexShaderBytecode(&bc, &sz);

        D3D11_INPUT_ELEMENT_DESC desc[] =
        {
            { "SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,
              0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,
              0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        dev->CreateInputLayout(desc, 2, bc, sz,
            m_debugLayout.GetAddressOf());

        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(DebugVC) * 4096;
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        dev->CreateBuffer(&bd, nullptr, m_boneVB.GetAddressOf());
        m_boneVBSize = bd.ByteWidth / sizeof(DebugVC);

        dev->Release();
    }

    // ---------- ライン頂点を組み立て ----------
    std::vector<DebugVC> lines;
    lines.reserve(data_.bones_.size() * 8);

    XMFLOAT4 col_bone(1.0f, 0.9f, 0.2f, 1.0f);
    XMFLOAT4 col_x(1, 0, 0, 1);
    XMFLOAT4 col_y(0, 1, 0, 1);
    XMFLOAT4 col_z(0, 0.5f, 1, 1);

    float axis_len = data_.scene_radius_ * 0.03f;
    if (axis_len < 0.02f) axis_len = 0.02f;

    for (size_t i = 0; i < data_.bones_.size(); ++i)
    {
        int parent = data_.bones_[i].parent;
        const XMFLOAT4X4& Wi = data_.curr_world_[i];

        if (parent >= 0)
        {
            XMFLOAT3 p0 = GetBonePosition(data_.curr_world_[parent]);
            XMFLOAT3 p1 = GetBonePosition(Wi);
            lines.push_back({ p0, col_bone });
            lines.push_back({ p1, col_bone });
        }

        XMFLOAT3 o = GetBonePosition(Wi);
        XMFLOAT3 x1 = TransformPoint(Wi, XMFLOAT3(axis_len, 0, 0));
        XMFLOAT3 y1 = TransformPoint(Wi, XMFLOAT3(0, axis_len, 0));
        XMFLOAT3 z1 = TransformPoint(Wi, XMFLOAT3(0, 0, axis_len));

        lines.push_back({ o,  col_x }); lines.push_back({ x1, col_x });
        lines.push_back({ o,  col_y }); lines.push_back({ y1, col_y });
        lines.push_back({ o,  col_z }); lines.push_back({ z1, col_z });
    }

    if (lines.empty()) return;

    // ---------- VB 再確保 ----------
    if (m_boneVBSize < lines.size())
    {
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

    // ---------- VB 書き込み ----------
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(ctx->Map(
        m_boneVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        std::memcpy(mapped.pData,
            lines.data(), lines.size() * sizeof(DebugVC));
        ctx->Unmap(m_boneVB.Get(), 0);
    }

    // ---------- DepthState: Z 完全 OFF で“上描き” ----------
    static Microsoft::WRL::ComPtr<ID3D11DepthStencilState> s_depthOff;

    if (!s_depthOff)
    {
        ID3D11Device* dev = nullptr;
        ctx->GetDevice(&dev);
        if (!dev) return;

        D3D11_DEPTH_STENCIL_DESC ds{};
        ds.DepthEnable = FALSE;                         // ★ Zテストしない
        ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;   // 書き込みなし
        ds.StencilEnable = FALSE;

        dev->CreateDepthStencilState(&ds, s_depthOff.GetAddressOf());
        dev->Release();
    }

    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> oldDSS;
    UINT oldRef = 0;
    ctx->OMGetDepthStencilState(oldDSS.GetAddressOf(), &oldRef);

    ctx->OMSetDepthStencilState(s_depthOff.Get(), 0);

    // ---------- 描画 ----------
    UINT stride = sizeof(DebugVC);
    UINT offset = 0;
    ID3D11Buffer* vb = m_boneVB.Get();

    ctx->IASetInputLayout(m_debugLayout.Get());
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    m_debugFx->SetWorld(world);
    m_debugFx->SetView(view);
    m_debugFx->SetProjection(proj);
    m_debugFx->Apply(ctx);

    ctx->Draw((UINT)lines.size(), 0);

    // DepthState を元に戻す
    ctx->OMSetDepthStencilState(oldDSS.Get(), oldRef);
}
