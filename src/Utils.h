#pragma once
#include <WICTextureLoader.h>
#include <DirectXMath.h>
#include <filesystem>
#include "UfbxStaticModel.h"
#include "ufbx.h"
#include "UfbxUtil.h"




namespace
{
    using namespace DirectX;
    namespace fs = std::filesystem;

    // ------------------------------------------------------------
    // デバッグ用頂点（ボーン表示のライン描画に使う）
    // ------------------------------------------------------------
    struct DebugVC
    {
        XMFLOAT3 pos; // 頂点位置
        XMFLOAT4 col; // 頂点カラー（RGBA）
    };

   
    // ------------------------------------------------------------
    // ボーンウェイトのソート用
    // ・std::sort の比較関数（重い順に並べる）
    // ------------------------------------------------------------
    bool CompareBoneWeightPair(
        const std::pair<uint16_t, float>& a,
        const std::pair<uint16_t, float>& b)
    {
        return a.second > b.second;
    }

    // ------------------------------------------------------------
    // CPU スキニング
    // ・各頂点について、ボーン行列とウェイトを使って新しい位置と法線を計算
    // ・GPU スキニングではなく、CPU で計算して VB を更新する方式
    // ------------------------------------------------------------
    void ApplySkinCPU(
        const std::vector<XMMATRIX>& skin_mats,
        const std::vector<UfbxStaticModel::VertexInfluence>& influences,
        const std::vector<UfbxStaticModel::VertexPNT2>& bind_vertices,
        std::vector<UfbxStaticModel::VertexPNT2>& out_vertices)
    {
        size_t n = bind_vertices.size();
        out_vertices.resize(n);

        size_t v;
        for (v = 0; v < n; ++v) {
            const UfbxStaticModel::VertexInfluence& inf = influences[v];

            XMVECTOR P = XMVectorZero(); // 合成された位置
            XMVECTOR N = XMVectorZero(); // 合成された法線
            bool any = false;            // 1つでも有効なボーンがあったか

            int k;
            for (k = 0; k < 4; ++k) {
                float w = inf.weight[k];
                uint16_t b = inf.bone[k];

                if (w <= 0.0f) continue;              // ウェイト0は無視
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
            UfbxStaticModel::VertexPNT2 sv = bind_vertices[v];
            XMStoreFloat3(&sv.pos, P);
            N = XMVector3Normalize(N);
            XMStoreFloat3(&sv.nrm, N);
            out_vertices[v] = sv;
        }
    }

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

    // ------------------------------------------------------------
    // node_to_world(t) 再帰ヘルパ
    // ・指定ノードの「アニメーション適用後のワールド行列」を計算する
    // ・親がいれば親のワールド行列も再帰的に計算
    // ・計算済みは cache に保存して再利用
    // ------------------------------------------------------------
    DirectX::XMFLOAT4X4 EvaluateNodeWorldRecursive(
        const ufbx_node* node,
        const ufbx_anim* anim,
        double t,
        std::unordered_map<const ufbx_node*, DirectX::XMFLOAT4X4>& cache)
    {
        using namespace DirectX;

        // すでに計算済みならキャッシュから返す
        std::unordered_map<const ufbx_node*, XMFLOAT4X4>::iterator it =
            cache.find(node);
        if (it != cache.end()) {
            return it->second;
        }

        // ローカル変換を取得
        ufbx_transform xf = ufbx_evaluate_transform(anim, node, t);
        ufbx_matrix    lm = ufbx_transform_to_matrix(&xf);
        XMFLOAT4X4     xm_local = UfbxUtil::ToXMMatrix(lm);
        XMMATRIX       L = XMLoadFloat4x4(&xm_local);

        XMMATRIX W;

        if (node->parent) {
            // 親がいれば親のワールド行列を先に計算
            XMFLOAT4X4 parent_world =
                EvaluateNodeWorldRecursive(node->parent, anim, t, cache);
            XMMATRIX PW = XMLoadFloat4x4(&parent_world);
            // 前の実装と同じ掛け順に合わせる
            W = L * PW;
        }
        else {
            // ルートノードはローカル = ワールド
            W = L;
        }

        XMFLOAT4X4 xm_world;
        XMStoreFloat4x4(&xm_world, W);

        // キャッシュに保存
        cache.insert(std::make_pair(node, xm_world));
        return xm_world;
    }



    // ------------------------------------------------------------
    // ボーン登録ヘルパ
    // ・ufbx_node を SkeletonData に追加し、そのインデックスを返す
    // ・すでに登録されている場合は既存のインデックスを返す
    // ------------------------------------------------------------
    int AddBoneInternal(
        const ufbx_node* node,
        UfbxStaticModel::SkeletonData& skeleton,
        std::unordered_map<const ufbx_node*, int>& index_of)
    {
        // すでに登録済みかチェック
        std::unordered_map<const ufbx_node*, int>::iterator it =
            index_of.find(node);
        if (it != index_of.end()) {
            return it->second;
        }

        // 新しいボーンとして追加
        int idx = static_cast<int>(skeleton.bones_.size());
        index_of.insert(std::make_pair(node, idx));

        UfbxStaticModel::BoneInfo info;
        info.node = node;
        skeleton.bones_.push_back(info);
        return idx;
    }
} // anonymous namespace