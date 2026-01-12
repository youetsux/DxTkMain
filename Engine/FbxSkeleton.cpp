#include "FbxSkeleton.h"
#include "ufbx.h"
#include "UfbxUtil.h"
#include "Gfx.h"        // DrawDebug �実装時に使う想定（今は未使用）
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


    // 旧デバッグ描画
    std::vector<BoneInfo>        m_bones;     // 旧の基本情報
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

            // 各クラスター（ボーン支配頂点群）の情報を見る
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
    if (anim->time_end > anim->time_begin)
    {
        if (t < anim->time_begin) t = anim->time_begin;
        if (t > anim->time_end)   t = anim->time_end;
    }

    // ------------------------------------------------------------
    // 単純ベイク再生（完全版）
    // ・毎フレーム ufbx_evaluate_scene() はしない
    // ・ufbx_evaluate_transform() も使わない
    // ・ufbx_bake_anim() で作った baked のキー列から TRS を補間して行列を作る
    // ・キーが無い成分は node->local_transform（レスト姿勢）を使う
    // ------------------------------------------------------------

    struct BakeCache
    {
        const ufbx_anim* anim = nullptr;
        ufbx_baked_anim* baked = nullptr;
        std::unordered_map<uint32_t, const ufbx_baked_node*> node_by_typed_id;
    };

    static std::unordered_map<const FbxSkeleton*, BakeCache> s_cache;

    BakeCache& cache = s_cache[this];

    if (!cache.baked || cache.anim != anim)
    {
        if (cache.baked)
        {
            ufbx_free_baked_anim(cache.baked);
            cache.baked = nullptr;
        }

        cache.anim = anim;
        cache.node_by_typed_id.clear();

        ufbx_error error = {};
        cache.baked = ufbx_bake_anim(scene, anim, nullptr, &error);
        if (!cache.baked)
        {
            // ベイクに失敗した場合：今回は「単純ベイク」方針のため、何も更新せずに戻る
            return;
        }

        cache.node_by_typed_id.reserve(cache.baked->nodes.count * 2);
        for (size_t i = 0; i < cache.baked->nodes.count; ++i)
        {
            const ufbx_baked_node* bn = &cache.baked->nodes.data[i];
            cache.node_by_typed_id[bn->typed_id] = bn;
        }
    }

    const size_t bone_count = data_.bones_.size();
    if (data_.curr_world_.size() != bone_count)
    {
        data_.curr_world_.resize(bone_count);
    }

    auto eval_vec3 = [](const ufbx_baked_vec3_list& keys, double time, ufbx_vec3 fallback) -> ufbx_vec3
        {
            if (keys.count == 0) return fallback;
            if (keys.count == 1) return keys.data[0].value;

            if (time <= keys.data[0].time) return keys.data[0].value;
            if (time >= keys.data[keys.count - 1].time) return keys.data[keys.count - 1].value;

            size_t lo = 0;
            size_t hi = keys.count - 1;
            while (hi - lo > 1)
            {
                size_t mid = (lo + hi) / 2;
                if (time < keys.data[mid].time) hi = mid;
                else lo = mid;
            }

            const ufbx_baked_vec3& a = keys.data[lo];
            const ufbx_baked_vec3& b = keys.data[hi];
            const double dt = b.time - a.time;
            const double k = dt > 0.0 ? (time - a.time) / dt : 0.0;

            ufbx_vec3 out;
            out.x = a.value.x + (b.value.x - a.value.x) * k;
            out.y = a.value.y + (b.value.y - a.value.y) * k;
            out.z = a.value.z + (b.value.z - a.value.z) * k;
            return out;
        };

    auto eval_quat = [](const ufbx_baked_quat_list& keys, double time, ufbx_quat fallback) -> ufbx_quat
        {
            if (keys.count == 0) return fallback;
            if (keys.count == 1) return keys.data[0].value;

            if (time <= keys.data[0].time) return keys.data[0].value;
            if (time >= keys.data[keys.count - 1].time) return keys.data[keys.count - 1].value;

            size_t lo = 0;
            size_t hi = keys.count - 1;
            while (hi - lo > 1)
            {
                size_t mid = (lo + hi) / 2;
                if (time < keys.data[mid].time) hi = mid;
                else lo = mid;
            }

            const ufbx_baked_quat& a = keys.data[lo];
            const ufbx_baked_quat& b = keys.data[hi];

            const double dt = b.time - a.time;
            const float k = (float)(dt > 0.0 ? (time - a.time) / dt : 0.0);

            using namespace DirectX;
            XMVECTOR qa = XMVectorSet((float)a.value.x, (float)a.value.y, (float)a.value.z, (float)a.value.w);
            XMVECTOR qb = XMVectorSet((float)b.value.x, (float)b.value.y, (float)b.value.z, (float)b.value.w);

            XMVECTOR q = XMQuaternionSlerp(qa, qb, k);
            q = XMQuaternionNormalize(q);

            XMFLOAT4 fq;
            XMStoreFloat4(&fq, q);

            ufbx_quat out;
            out.x = fq.x; out.y = fq.y; out.z = fq.z; out.w = fq.w;
            return out;
        };

    // ワールド合成：Parent * Local（あなたの実装規約に合わせる）
    for (size_t i = 0; i < bone_count; ++i)
    {
        const BoneInfo& bone = data_.bones_[i];
        if (!bone.node)
        {
            DirectX::XMStoreFloat4x4(&data_.curr_world_[i], DirectX::XMMatrixIdentity());
            continue;
        }

        // レスト姿勢（デフォルト値）
        ufbx_transform xf = bone.node->local_transform;

        // ベイク済みキーで上書き
        std::unordered_map<uint32_t, const ufbx_baked_node*>::const_iterator it =
            cache.node_by_typed_id.find(bone.node->typed_id);

        if (it != cache.node_by_typed_id.end())
        {
            const ufbx_baked_node* bn = it->second;

            xf.translation = eval_vec3(bn->translation_keys, t, xf.translation);
            xf.rotation = eval_quat(bn->rotation_keys, t, xf.rotation);
            xf.scale = eval_vec3(bn->scale_keys, t, xf.scale);
        }

        const ufbx_matrix lm = ufbx_transform_to_matrix(&xf);
        const DirectX::XMFLOAT4X4 xm_local = UfbxUtil::ToXMMatrix(lm);
        const DirectX::XMMATRIX L = DirectX::XMLoadFloat4x4(&xm_local);

        DirectX::XMMATRIX W = L;
        if (bone.parent >= 0)
        {
            const DirectX::XMFLOAT4X4& parentWorld = data_.curr_world_[static_cast<size_t>(bone.parent)];
            const DirectX::XMMATRIX PW = DirectX::XMLoadFloat4x4(&parentWorld);
            W = L*PW;
        }

        DirectX::XMStoreFloat4x4(&data_.curr_world_[i], W);
    }
}




namespace {
    // ------------------------------------------------------------
    // 旧の位置/変換ヘルパ（DrawSkeleton で使う）
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

    // ---------- DepthState: Z 完全 OFF で常時描き！ ----------
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

    ctx->IASetInputLayout(m_debugLayout.Get());
    ctx->IASetVertexBuffers(0, 1, m_boneVB.GetAddressOf(), &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    m_debugFx->SetWorld(world);
    m_debugFx->SetView(view);
    m_debugFx->SetProjection(proj);

    m_debugFx->Apply(ctx);
    ctx->Draw((UINT)lines.size(), 0);

    // depth state 戻す
    ctx->OMSetDepthStencilState(oldDSS.Get(), oldRef);
}
