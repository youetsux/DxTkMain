#include "FbxSkeleton.h"

#include "ufbx.h"
#include "UfbxUtil.h"
#include "Gfx.h"

#include <d3d11.h>
#include <wrl/client.h>
#include <Effects.h>

#include <unordered_map>
#include <vector>
#include <cstring>

//------------------------------------------------------------
// 内部ヘルパ（匿名名前空間）
//------------------------------------------------------------
namespace
{
    // ボーン登録ヘルパ
    int AddBoneInternal(
        const ufbx_node* node,
        SkeletonData& skeleton,
        std::unordered_map<const ufbx_node*, int>& index_of)
    {
        auto it = index_of.find(node);
        if (it != index_of.end()) {
            return it->second;
        }

        int idx = static_cast<int>(skeleton.bones_.size());
        index_of.insert(std::make_pair(node, idx));

        BoneInfo info;
        info.node = node;
        skeleton.bones_.push_back(info);
        return idx;
    }

    // ------------------------------------------------------------
    // DrawDebug 用の静的キャッシュ（FbxSkeleton に押し付けない）
    // ------------------------------------------------------------
    struct DebugVC
    {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT4 col;
    };

    std::unique_ptr<DirectX::BasicEffect>               g_debugFx;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>           g_debugLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer>                g_boneVB;
    size_t                                              g_boneVBVerts = 0; // 頂点数
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>     g_depthOff;

    DirectX::XMFLOAT3 GetBonePosition(const DirectX::XMFLOAT4X4& M)
    {
        DirectX::XMFLOAT3 p;
        p.x = M._41;
        p.y = M._42;
        p.z = M._43;
        return p;
    }

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

    void EnsureDebugResources(ID3D11DeviceContext* ctx)
    {
        if (!ctx) return;

        if (!g_debugFx)
        {
            ID3D11Device* dev = nullptr;
            ctx->GetDevice(&dev);
            if (!dev) return;

            g_debugFx = std::make_unique<DirectX::BasicEffect>(dev);
            g_debugFx->SetVertexColorEnabled(true);
            g_debugFx->SetLightingEnabled(false);

            const void* bc = nullptr;
            size_t      sz = 0;
            g_debugFx->GetVertexShaderBytecode(&bc, &sz);

            D3D11_INPUT_ELEMENT_DESC desc[] =
            {
                { "SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,
                  0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
                { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,
                  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            };

            dev->CreateInputLayout(desc, 2, bc, sz, g_debugLayout.GetAddressOf());

            // 初期 VB（必要なら後で拡張）
            D3D11_BUFFER_DESC bd{};
            bd.ByteWidth = sizeof(DebugVC) * 4096;
            bd.Usage = D3D11_USAGE_DYNAMIC;
            bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            dev->CreateBuffer(&bd, nullptr, g_boneVB.GetAddressOf());
            g_boneVBVerts = bd.ByteWidth / sizeof(DebugVC);

            dev->Release();
        }

        if (!g_depthOff)
        {
            ID3D11Device* dev = nullptr;
            ctx->GetDevice(&dev);
            if (!dev) return;

            D3D11_DEPTH_STENCIL_DESC ds{};
            ds.DepthEnable = FALSE;
            ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
            ds.DepthFunc = D3D11_COMPARISON_ALWAYS;
            ds.StencilEnable = FALSE;

            dev->CreateDepthStencilState(&ds, g_depthOff.GetAddressOf());
            dev->Release();
        }
    }

    void EnsureVBSize(ID3D11DeviceContext* ctx, size_t requiredVerts)
    {
        if (!ctx) return;
        if (g_boneVB && g_boneVBVerts >= requiredVerts) return;

        g_boneVB.Reset();

        ID3D11Device* dev = nullptr;
        ctx->GetDevice(&dev);
        if (!dev) return;

        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = static_cast<UINT>(requiredVerts * sizeof(DebugVC));
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        dev->CreateBuffer(&bd, nullptr, g_boneVB.GetAddressOf());
        g_boneVBVerts = requiredVerts;

        dev->Release();
    }
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


//============================================================
// ApplyBakedPoseWorld
//   - scene を保持しない運用向け: ベイク済み world 行列配列をそのまま適用
//============================================================
void FbxSkeleton::ApplyBakedPoseWorld(const DirectX::XMFLOAT4X4* worlds, size_t bone_count)
{
    if (!worlds) return;
    if (bone_count == 0) return;
    if (data_.bones_.size() != bone_count) return;

    if (data_.curr_world_.size() != bone_count) {
        data_.curr_world_.resize(bone_count);
    }

    for (size_t i = 0; i < bone_count; ++i) {
        data_.curr_world_[i] = worlds[i];
    }
}

//============================================================
// DetachFromScene
//   - ufbx_scene 破棄前に呼び、scene 由来ポインタを無効化する
//============================================================
void FbxSkeleton::DetachFromScene()
{
    for (auto& b : data_.bones_) {
        b.node = nullptr;
    }
    data_.bone_index_of_.clear();
}
bool FbxSkeleton::BuildFromScene(const ufbx_scene* scene)
{
    data_.bones_.clear();
    data_.curr_world_.clear();
    data_.bone_index_of_.clear();
    data_.skin_mats_.clear();
    data_.scene_radius_ = 1.0f;

    if (!scene) {
        return false;
    }

    std::unordered_map<const ufbx_node*, int> index_of;

    for (size_t ni = 0; ni < scene->nodes.count; ++ni) {
        const ufbx_node* node = scene->nodes.data[ni];
        const ufbx_mesh* mesh = node->mesh;
        if (!mesh) continue;

        for (size_t si = 0; si < mesh->skin_deformers.count; ++si) {
            const ufbx_skin_deformer* skin = mesh->skin_deformers.data[si];

            for (size_t ci = 0; ci < skin->clusters.count; ++ci) {
                const ufbx_skin_cluster* cl = skin->clusters.data[ci];
                if (!cl->bone_node) continue;

                int bi = AddBoneInternal(cl->bone_node, data_, index_of);

                data_.bones_[bi].bind_world = UfbxUtil::ToXMMatrix(cl->bind_to_world);

                {
                    DirectX::XMMATRIX B = DirectX::XMLoadFloat4x4(&data_.bones_[bi].bind_world);
                    DirectX::XMMATRIX B_inv = DirectX::XMMatrixInverse(nullptr, B);
                    DirectX::XMStoreFloat4x4(&data_.bones_[bi].inv_bind_world, B_inv);
                }

                data_.bones_[bi].geom_bind_world = UfbxUtil::ToXMMatrix(cl->geometry_to_bone);

                {
                    DirectX::XMMATRIX G = DirectX::XMLoadFloat4x4(&data_.bones_[bi].geom_bind_world);
                    DirectX::XMMATRIX G_inv = DirectX::XMMatrixInverse(nullptr, G);
                    DirectX::XMStoreFloat4x4(&data_.bones_[bi].inv_geom_bind_world, G_inv);
                }

                if (cl->bone_node->parent) {
                    auto itp = index_of.find(cl->bone_node->parent);
                    if (itp != index_of.end()) {
                        data_.bones_[bi].parent = itp->second;
                    }
                }
            }
        }
    }

    if (data_.bones_.empty()) {
        return true;
    }

    data_.curr_world_.resize(data_.bones_.size());
    data_.skin_mats_.resize(data_.bones_.size());

    for (size_t i = 0; i < data_.bones_.size(); ++i) {
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

    double t = t_sec;
    if (anim->time_end > anim->time_begin)
    {
        if (t < anim->time_begin) t = anim->time_begin;
        if (t > anim->time_end)   t = anim->time_end;
    }

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

    for (size_t i = 0; i < bone_count; ++i)
    {
        const BoneInfo& bone = data_.bones_[i];
        if (!bone.node)
        {
            DirectX::XMStoreFloat4x4(&data_.curr_world_[i], DirectX::XMMatrixIdentity());
            continue;
        }

        ufbx_transform xf = bone.node->local_transform;

        auto it = cache.node_by_typed_id.find(bone.node->typed_id);
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
            W = L * PW; // ※元コードの規約を維持
        }

        DirectX::XMStoreFloat4x4(&data_.curr_world_[i], W);
    }
}

void FbxSkeleton::DrawDebug(
    const DirectX::XMMATRIX& world,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& proj)
{
    using namespace DirectX;

    ID3D11DeviceContext* ctx = Gfx::Ctx();
    if (!ctx) return;
    if (data_.bones_.empty()) return;

    EnsureDebugResources(ctx);

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
            XMFLOAT3 p0 = GetBonePosition(data_.curr_world_[static_cast<size_t>(parent)]);
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

    EnsureVBSize(ctx, lines.size());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(ctx->Map(g_boneVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        std::memcpy(mapped.pData, lines.data(), lines.size() * sizeof(DebugVC));
        ctx->Unmap(g_boneVB.Get(), 0);
    }

    // Depth state 保存→Z完全OFF→描画→復帰
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> oldDSS;
    UINT oldRef = 0;
    ctx->OMGetDepthStencilState(oldDSS.GetAddressOf(), &oldRef);
    ctx->OMSetDepthStencilState(g_depthOff.Get(), 0);

    UINT stride = sizeof(DebugVC);
    UINT offset = 0;

    ctx->IASetInputLayout(g_debugLayout.Get());
    ID3D11Buffer* vb = g_boneVB.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    g_debugFx->SetWorld(world);
    g_debugFx->SetView(view);
    g_debugFx->SetProjection(proj);
    g_debugFx->Apply(ctx);

    ctx->Draw(static_cast<UINT>(lines.size()), 0);

    ctx->OMSetDepthStencilState(oldDSS.Get(), oldRef);
}
