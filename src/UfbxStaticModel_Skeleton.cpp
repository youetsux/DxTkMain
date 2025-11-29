//#include "UfbxStaticModel.h"
//
//#include <Windows.h>
//#include <filesystem>
//#include <unordered_map>
//#include <string>
//#include <cstring>
//
//#include <WICTextureLoader.h>
//#include "Gfx.h"
//#include "ufbx.h"
//#include "UfbxUtil.h"
//#include "FbxSkeleton.h"
//
//using Microsoft::WRL::ComPtr;
//
//namespace
//{
//    using namespace DirectX;
//
//    // ------------------------------------------------------------
//    // デバッグ用頂点（ボーン表示のライン描画に使う）
//    // ------------------------------------------------------------
//    struct DebugVC
//    {
//        XMFLOAT3 pos; // 頂点位置
//        XMFLOAT4 col; // 頂点カラー（RGBA）
//    };
//
//    // ------------------------------------------------------------
//    // 骨の位置ヘルパ（DrawSkeleton で使う）
//    // ------------------------------------------------------------
//    DirectX::XMFLOAT3 GetBonePosition(const DirectX::XMFLOAT4X4& M)
//    {
//        // 行列の第4行の xyz が平行移動成分
//        DirectX::XMFLOAT3 p;
//        p.x = M._41;
//        p.y = M._42;
//        p.z = M._43;
//        return p;
//    }
//
//    // ------------------------------------------------------------
//    // ボーン登録ヘルパ
//    // ・ufbx_node を SkeletonData に追加し、そのインデックスを返す
//    // ・すでに登録されている場合は既存のインデックスを返す
//    // ------------------------------------------------------------
//    int AddBoneInternal(
//        const ufbx_node* node,
//        SkeletonData& skeleton,
//        std::unordered_map<const ufbx_node*, int>& index_of)
//    {
//        // すでに登録済みかチェック
//        std::unordered_map<const ufbx_node*, int>::iterator it =
//            index_of.find(node);
//        if (it != index_of.end()) {
//            return it->second;
//        }
//
//        // 新しいボーンとして追加
//        int idx = static_cast<int>(skeleton.bones_.size());
//        index_of.insert(std::make_pair(node, idx));
//
//        BoneInfo info;
//        info.node = node;
//        skeleton.bones_.push_back(info);
//        return idx;
//    }
//} // anonymous namespace
//
////================================================================
//// スケルトン構築
//// ・シーン中のスキン情報からボーンとそのバインドポーズを取り出す
////================================================================
////bool UfbxStaticModel::BuildSkeletonFromScene(const ufbx_scene* scene)
////{
////    // いったん全部クリア
////    skeleton_.bones_.clear();
////    skeleton_.curr_world_.clear();
////    skeleton_.bone_index_of_.clear();
////    skeleton_.skin_mats_.clear();
////
////    if (!scene) {
////        return false;
////    }
////
////    // ufbx_node* → ボーンインデックス の一時マップ
////    std::unordered_map<const ufbx_node*, int> index_of;
////
////    // 全ノードをループ
////    size_t ni;
////    for (ni = 0; ni < scene->nodes.count; ++ni) {
////        const ufbx_node* node = scene->nodes.data[ni];
////        const ufbx_mesh* mesh = node->mesh;
////        if (!mesh) continue; // メッシュを持たないノードはスキップ
////
////        // メッシュについているスキンデフォーマを全て見る
////        size_t si;
////        for (si = 0; si < mesh->skin_deformers.count; ++si) {
////            const ufbx_skin_deformer* skin =
////                mesh->skin_deformers.data[si];
////
////            // 各クラスター（ボーン＋頂点群）の情報を見る
////            size_t ci;
////            for (ci = 0; ci < skin->clusters.count; ++ci) {
////                const ufbx_skin_cluster* cl =
////                    skin->clusters.data[ci];
////                if (!cl->bone_node) continue;
////
////                // このクラスターのボーンを SkeletonData に登録
////                int bi = AddBoneInternal(
////                    cl->bone_node, skeleton_, index_of);
////
////                // ボーン側のバインド姿勢（ボーンのワールド行列）
////                skeleton_.bones_[bi].bind_world =
////                    UfbxUtil::ToXMMatrix(cl->bind_to_world);
////
////                // バインド姿勢の逆行列も作っておく
////                {
////                    DirectX::XMMATRIX B =
////                        DirectX::XMLoadFloat4x4(&skeleton_.bones_[bi].bind_world);
////                    DirectX::XMMATRIX B_inv = DirectX::XMMatrixInverse(nullptr, B);
////                    DirectX::XMStoreFloat4x4(
////                        &skeleton_.bones_[bi].inv_bind_world, B_inv);
////                }
////
////                // ジオメトリ → ボーン の変換行列
////                skeleton_.bones_[bi].geom_bind_world =
////                    UfbxUtil::ToXMMatrix(cl->geometry_to_bone);
////
////                // その逆行列
////                {
////                    DirectX::XMMATRIX G =
////                        DirectX::XMLoadFloat4x4(&skeleton_.bones_[bi].geom_bind_world);
////                    DirectX::XMMATRIX G_inv = DirectX::XMMatrixInverse(nullptr, G);
////                    DirectX::XMStoreFloat4x4(
////                        &skeleton_.bones_[bi].inv_geom_bind_world, G_inv);
////                }
////
////                // 親ボーンとの関係（親があればインデックスを調べてセット）
////                if (cl->bone_node->parent) {
////                    std::unordered_map<const ufbx_node*, int>::iterator itp =
////                        index_of.find(cl->bone_node->parent);
////                    if (itp != index_of.end()) {
////                        skeleton_.bones_[bi].parent = itp->second;
////                    }
////                }
////            }
////        }
////    }
////
////    // ボーンが 1 本も無ければ、そのまま true（メッシュだけのモデル対応）
////    if (skeleton_.bones_.empty()) {
////        return true;
////    }
////
////    // 現在姿勢・スキン行列用配列をボーン数に合わせて確保
////    skeleton_.curr_world_.resize(skeleton_.bones_.size());
////    skeleton_.skin_mats_.resize(skeleton_.bones_.size());
////
////    // ufbx_node* → uint16_t ボーン番号 のマップを作る
////    uint16_t i16 = 0;
////    size_t i;
////    for (i = 0; i < skeleton_.bones_.size(); ++i) {
////        // 初期姿勢はバインド姿勢
////        skeleton_.curr_world_[i] = skeleton_.bones_[i].bind_world;
////        if (skeleton_.bones_[i].node && i < 0x10000) {
////            i16 = static_cast<uint16_t>(i);
////            skeleton_.bone_index_of_[skeleton_.bones_[i].node] = i16;
////        }
////    }
////
////    return true;
////}
//
////================================================================
//// アニメ評価（オーバーロード）
////================================================================
//
//// アニメ評価（オーバーロードその1）: scene->anim（デフォルトのアニメ）を使う
//void UfbxStaticModel::UpdateSkeletonAtTime(
//    const ufbx_scene* scene,
//    double            t_sec)
//{
//    const ufbx_anim* anim = scene ? scene->anim : nullptr;
//    UpdateSkeletonAtTime(scene, anim, t_sec);
//}
//
//// アニメ評価（オーバーロードその2）: アニメーションスタックをインデックスで選ぶ
//void UfbxStaticModel::UpdateSkeletonAtTime(
//    const ufbx_scene* scene,
//    size_t            stack_index,
//    double            t_sec)
//{
//    const ufbx_anim* anim = nullptr;
//
//    if (scene && stack_index < scene->anim_stacks.count) {
//        const ufbx_anim_stack* st =
//            scene->anim_stacks.data[stack_index];
//        if (st) {
//            anim = st->anim;
//        }
//    }
//    UpdateSkeletonAtTime(scene, anim, t_sec);
//}
//
//// アニメ評価（オーバーロードその3）: アニメーションスタックを名前で選ぶ
//void UfbxStaticModel::UpdateSkeletonAtTime(
//    const ufbx_scene* scene,
//    const std::string& stack_name,
//    double             t_sec)
//{
//    const ufbx_anim* anim = nullptr;
//
//    if (scene && !stack_name.empty()) {
//        size_t i;
//        for (i = 0; i < scene->anim_stacks.count; ++i) {
//            const ufbx_anim_stack* st =
//                scene->anim_stacks.data[i];
//            if (!st || !st->name.data) continue;
//
//            if (std::strcmp(st->name.data, stack_name.c_str()) == 0) {
//                anim = st->anim;
//                break;
//            }
//        }
//    }
//    UpdateSkeletonAtTime(scene, anim, t_sec);
//}
//
//// デフォルトアニメ取得
//const ufbx_anim* UfbxStaticModel::GetDefaultAnim() const
//{
//    return scene_ ? scene_.get()->anim : nullptr;
//}
//
//// scene_ を使う簡易ラッパ
//void UfbxStaticModel::UpdateSkeletonAtTime(double t_sec)
//{
//    // 内部で管理している scene_ を使う
//    UpdateSkeletonAtTime(scene_.get(), t_sec);
//}
//
//
//
//
////----------------------------------------------------------------
//// アニメ評価本体
//// ・指定時刻 t_sec における各ボーンのワールド行列を計算
////   → 実際の評価処理は FbxSkeleton に委譲する
////----------------------------------------------------------------
//void UfbxStaticModel::UpdateSkeletonAtTime(
//    const ufbx_scene* scene,
//    const ufbx_anim* anim,
//    double            t_sec)
//{
//    if (!scene || !anim) return;
//
//    // FbxSkeleton 側に「scene + anim + t_sec」で丸投げ
//    skeleton_.UpdateAtTime(scene, anim, t_sec);
//}
//
//
////----------------------------------------------------------------
//// アニメ評価本体
//// ・指定時刻 t_sec における各ボーンのワールド行列を計算し
////   skeleton_.curr_world_ に書き込む
////----------------------------------------------------------------
////void UfbxStaticModel::UpdateSkeletonAtTime(
////    const ufbx_scene* scene,
////    const ufbx_anim* anim,
////    double            t_sec)
////{
////    if (!scene || !anim) return;
////    if (skeleton_.bones_.empty()) return;
////
////    // t をアニメーション時間の範囲にクランプ
////    double t = t_sec;
////    if (anim->time_end > anim->time_begin) {
////        if (t < anim->time_begin) t = anim->time_begin;
////        if (t > anim->time_end)   t = anim->time_end;
////    }
////
////    // 計算済みノードの結果をためるキャッシュ
////    std::unordered_map<const ufbx_node*, DirectX::XMFLOAT4X4> cache;
////
////    skeleton_.curr_world_.resize(skeleton_.bones_.size());
////
////    size_t i;
////    for (i = 0; i < skeleton_.bones_.size(); ++i) {
////        const ufbx_node* node = skeleton_.bones_[i].node;
////        if (!node) continue;
////
////        // ボーンに対応するノードのワールド行列を求める
////        skeleton_.curr_world_[i] =
////            UfbxUtil::EvaluateNodeWorldRecursive(node, anim, t, cache);
////    }
////}
//
////================================================================
//// 骨デバッグ描画
//// ・ボーン同士を線で結んで表示
//// ・各ボーンのローカル軸も x/y/z 別の色で表示
////================================================================
////void UfbxStaticModel::DrawSkeleton(
////    const DirectX::XMMATRIX& world,
////    const DirectX::XMMATRIX& view,
////    const DirectX::XMMATRIX& proj)
////{
////    using namespace DirectX;
////
////    ID3D11DeviceContext* ctx = Gfx::Ctx();
////    if (!ctx) return;
////
////    if (skeleton_.bones_.empty()) return;
////
////    // デバッグ描画用のリソースを初期化（初回のみ）
////    if (!draw_.debug_fx_) {
////        ID3D11Device* dev = Gfx::Dev();
////        if (!dev) return;
////
////        draw_.debug_fx_.reset(new BasicEffect(dev));
////        draw_.debug_fx_->SetVertexColorEnabled(true);
////        draw_.debug_fx_->SetLightingEnabled(false);
////
////        const void* bc = nullptr;
////        size_t      sz = 0;
////        draw_.debug_fx_->GetVertexShaderBytecode(&bc, &sz);
////
////        D3D11_INPUT_ELEMENT_DESC il[] =
////        {
////            { "SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,
////              0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
////            { "COLOR",       0, DXGI_FORMAT_R32G32B32A32_FLOAT,
////              0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
////        };
////
////        dev->CreateInputLayout(
////            il, 2, bc, sz, draw_.debug_layout_.GetAddressOf());
////
////        D3D11_BUFFER_DESC bd;
////        std::memset(&bd, 0, sizeof(bd));
////        bd.ByteWidth = sizeof(DebugVC) * 4096;
////        bd.Usage = D3D11_USAGE_DYNAMIC;
////        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
////        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
////
////        dev->CreateBuffer(&bd, nullptr, draw_.bone_vb_.GetAddressOf());
////        draw_.bone_vb_size_ = bd.ByteWidth / sizeof(DebugVC);
////    }
////
////    // ライン頂点をためるバッファ
////    std::vector<DebugVC> lines;
////    lines.reserve(skeleton_.bones_.size() * 8);
////
////    XMFLOAT4 col_bone(1.0f, 0.9f, 0.2f, 1.0f);
////    XMFLOAT4 col_x(1, 0, 0, 1);
////    XMFLOAT4 col_y(0, 1, 0, 1);
////    XMFLOAT4 col_z(0, 0.5f, 1, 1);
////
////    // ボーン軸の長さ（モデルのサイズに合わせて調整）
////    float axis_len = skeleton_.scene_radius_ * 0.03f;
////    if (axis_len < 0.02f) axis_len = 0.02f;
////
////    // 各ボーンについてラインを登録
////    size_t i;
////    for (i = 0; i < skeleton_.bones_.size(); ++i) {
////        int parent = skeleton_.bones_[i].parent;
////        const XMFLOAT4X4& Wi = skeleton_.curr_world_[i];
////
////        // 親がいれば、親→子のラインを引く
////        if (parent >= 0) {
////            XMFLOAT3 p0 = GetBonePosition(
////                skeleton_.curr_world_[parent]);
////            XMFLOAT3 p1 = GetBonePosition(Wi);
////            lines.push_back(DebugVC{ p0, col_bone });
////            lines.push_back(DebugVC{ p1, col_bone });
////        }
////
////        // ボーンの軸（X,Y,Z）を描く
////        XMFLOAT3 o = GetBonePosition(Wi);
////        XMFLOAT3 x1 = UfbxUtil::TransformPoint(
////            Wi, XMFLOAT3(axis_len, 0, 0));
////        XMFLOAT3 y1 = UfbxUtil::TransformPoint(
////            Wi, XMFLOAT3(0, axis_len, 0));
////        XMFLOAT3 z1 = UfbxUtil::TransformPoint(
////            Wi, XMFLOAT3(0, 0, axis_len));
////
////        lines.push_back(DebugVC{ o,  col_x }); lines.push_back(DebugVC{ x1, col_x });
////        lines.push_back(DebugVC{ o,  col_y }); lines.push_back(DebugVC{ y1, col_y });
////        lines.push_back(DebugVC{ o,  col_z }); lines.push_back(DebugVC{ z1, col_z });
////    }
////
////    if (lines.empty()) return;
////
////    // 必要ならボーン用 VB を作り直す（頂点数が足りないとき）
////    if (draw_.bone_vb_size_ < lines.size()) {
////        draw_.bone_vb_.Reset();
////
////        ID3D11Device* dev = Gfx::Dev();
////        if (!dev) return;
////
////        D3D11_BUFFER_DESC bd;
////        std::memset(&bd, 0, sizeof(bd));
////        bd.ByteWidth = (UINT)(lines.size() * sizeof(DebugVC));
////        bd.Usage = D3D11_USAGE_DYNAMIC;
////        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
////        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
////
////        dev->CreateBuffer(&bd, nullptr, draw_.bone_vb_.GetAddressOf());
////        draw_.bone_vb_size_ = lines.size();
////    }
////
////    // VB にラインデータを書き込む
////    D3D11_MAPPED_SUBRESOURCE mapped;
////    if (SUCCEEDED(ctx->Map(
////        draw_.bone_vb_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
////    {
////        std::memcpy(mapped.pData,
////            &lines[0], lines.size() * sizeof(DebugVC));
////        ctx->Unmap(draw_.bone_vb_.Get(), 0);
////    }
////
////    // IA 設定してライン描画
////    UINT stride = sizeof(DebugVC);
////    UINT offset = 0;
////    ID3D11Buffer* vb = draw_.bone_vb_.Get();
////
////    ctx->IASetInputLayout(draw_.debug_layout_.Get());
////    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
////    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
////
////    draw_.debug_fx_->SetWorld(world);
////    draw_.debug_fx_->SetView(view);
////    draw_.debug_fx_->SetProjection(proj);
////    draw_.debug_fx_->Apply(ctx);
////
////    ctx->Draw((UINT)lines.size(), 0);
////}

#include "UfbxStaticModel.h"
#include "FbxSkeleton.h"
#include "ufbx.h"
#include "UfbxUtil.h"

#include <unordered_map>
#include <string>
#include <cstring>
#include <vector>
#include <cmath>

// …ここに FbxSkeleton の定義や、BuildFromScene / UpdateAtTime / DrawDebug などがある想定…

//================================================================
// UfbxStaticModel 側のスケルトン用ラッパ関数
//================================================================

// デフォルトアニメ取得
const ufbx_anim* UfbxStaticModel::GetDefaultAnim() const
{
    return scene_ ? scene_.get()->anim : nullptr;
}

// scene_ を使う簡易ラッパ（Model.cpp から呼ばれる）
void UfbxStaticModel::UpdateSkeletonAtTime(double t_sec)
{
    UpdateSkeletonAtTime(scene_.get(), t_sec);
}

//----------------------------------------------------------------
// アニメ評価（オーバーロードその1）
// ・scene->anim（デフォルトのアニメ）を使う
//----------------------------------------------------------------
void UfbxStaticModel::UpdateSkeletonAtTime(
    const ufbx_scene* scene,
    double            t_sec)
{
    const ufbx_anim* anim = scene ? scene->anim : nullptr;
    UpdateSkeletonAtTime(scene, anim, t_sec);
}

//----------------------------------------------------------------
// アニメ評価（オーバーロードその2）
// ・アニメーションスタックをインデックスで選ぶ
//----------------------------------------------------------------
void UfbxStaticModel::UpdateSkeletonAtTime(
    const ufbx_scene* scene,
    size_t            stack_index,
    double            t_sec)
{
    const ufbx_anim* anim = nullptr;

    if (scene && stack_index < scene->anim_stacks.count) {
        const ufbx_anim_stack* st = scene->anim_stacks.data[stack_index];
        if (st) {
            anim = st->anim;
        }
    }
    UpdateSkeletonAtTime(scene, anim, t_sec);
}

//----------------------------------------------------------------
// アニメ評価（オーバーロードその3）
// ・アニメーションスタックを名前で選ぶ
//----------------------------------------------------------------
void UfbxStaticModel::UpdateSkeletonAtTime(
    const ufbx_scene* scene,
    const std::string& stack_name,
    double             t_sec)
{
    const ufbx_anim* anim = nullptr;

    if (scene && !stack_name.empty()) {
        for (size_t i = 0; i < scene->anim_stacks.count; ++i) {
            const ufbx_anim_stack* st = scene->anim_stacks.data[i];
            if (!st || !st->name.data) continue;

            if (std::strcmp(st->name.data, stack_name.c_str()) == 0) {
                anim = st->anim;
                break;
            }
        }
    }
    UpdateSkeletonAtTime(scene, anim, t_sec);
}

//----------------------------------------------------------------
// アニメ評価本体
// ・指定時刻 t_sec における各ボーンのワールド行列を計算
//   → 実際の計算は FbxSkeleton に委譲
//----------------------------------------------------------------
void UfbxStaticModel::UpdateSkeletonAtTime(
    const ufbx_scene* scene,
    const ufbx_anim* anim,
    double            t_sec)
{
    if (!scene || !anim) return;

    // FbxSkeleton 側に丸投げ
    skeleton_.UpdateAtTime(scene, anim, t_sec);
}

//----------------------------------------------------------------
// スケルトンのデバッグ描画ラッパ
//----------------------------------------------------------------
void UfbxStaticModel::DrawSkeleton(
    const DirectX::XMMATRIX& world,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& proj)
{
    skeleton_.DrawDebug(world, view, proj);
}
