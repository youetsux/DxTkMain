#include "UfbxStaticModel.h"

#include <Windows.h>
#include <filesystem>
#include <unordered_map>
#include <string>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cfloat>

#include <WICTextureLoader.h>
#include "Gfx.h"
#include "ufbx.h"


using Microsoft::WRL::ComPtr;

// 無名名前空間
// ・この cpp ファイルの中だけで使う関数・構造体を閉じ込める場所
// ・外からは見えない（static と似た効果）
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
    // UTF-8 → std::filesystem::path 変換
    // ・FBX 内の文字列は UTF-8 なので、それを path に変換する
    // ------------------------------------------------------------
    fs::path PathFromUtf8(const char* bytes, size_t len)
    {
        const char8_t* p = reinterpret_cast<const char8_t*>(bytes);
        return fs::path(std::u8string(p, p + len));
    }

    // ufbx_string からパスを作る
    fs::path PathFromUfbx(const ufbx_string& s)
    {
        if (!s.data || s.length == 0) return fs::path();
        return PathFromUtf8(s.data, s.length);
    }

    // ufbx_string から「ファイル名だけ」取り出す
    fs::path FileNameFromUfbx(const ufbx_string& s)
    {
        return PathFromUfbx(s).filename();
    }

    // ------------------------------------------------------------
    // ufbx_string の内容比較
    // ・長さと中身のバイト列を比較する
    // ------------------------------------------------------------
    bool UfbxStringEquals(const ufbx_string& a, const ufbx_string& b)
    {
        if (a.length != b.length || !a.data || !b.data) return false;
        return std::strncmp(a.data, b.data, a.length) == 0;
    }

    // ------------------------------------------------------------
    // Diffuse 相当のテクスチャを取得
    // ・PBR, FBX の両方のプロパティを見て「メインカラー用テクスチャ」を探す
    // ------------------------------------------------------------
    const ufbx_texture* GetDiffuseTexture(const ufbx_material* mat)
    {
        if (!mat) return nullptr;
        if (mat->pbr.base_color.texture)    return mat->pbr.base_color.texture;
        if (mat->fbx.diffuse_color.texture) return mat->fbx.diffuse_color.texture;
        if (mat->fbx.ambient_color.texture) return mat->fbx.ambient_color.texture;
        return nullptr;
    }

    // ------------------------------------------------------------
    // UV セット名から vertex_uv を取得
    // ・Maya などで複数 UV セットを持つ場合に、テクスチャが参照している
    //   UV セットを見つけるために使う
    // ------------------------------------------------------------
    const ufbx_vertex_vec2* ResolveUVByName(
        const ufbx_mesh* mesh,
        const ufbx_string& uv_set_name)
    {
        if (!mesh || uv_set_name.length == 0) return nullptr;

        size_t i;
        for (i = 0; i < mesh->uv_sets.count; ++i) {
            const ufbx_uv_set& us = mesh->uv_sets.data[i];
            if (us.vertex_uv.exists && UfbxStringEquals(us.name, uv_set_name)) {
                return &us.vertex_uv;
            }
        }
        return nullptr;
    }

    // ------------------------------------------------------------
    // ufbx 属性のインデックス解決
    // ・FBX は「頂点ごと」「コーナーごと」などいろいろな持ち方をする
    // ・どの index を見ればいいかをここで統一して決める
    // ------------------------------------------------------------
    template<typename AttrT>
    uint32_t ValueIndexOf(const AttrT& attr, uint32_t corner, uint32_t vtx)
    {
        // 別の indices 配列を持っている場合
        if (attr.indices.count > 0)  return attr.indices.data[corner];
        // 頂点ごとに一意な場合
        if (attr.unique_per_vertex)  return vtx;
        // それ以外は corner 番号をそのまま使う
        return corner;
    }

    // ------------------------------------------------------------
    // ufbx_matrix → XMFLOAT4X4 変換
    // ・ufbx の行列を DirectX 用の行列に詰め替える
    // ------------------------------------------------------------
    DirectX::XMFLOAT4X4 ToXMMatrix(const ufbx_matrix& m)
    {
        DirectX::XMFLOAT4X4 out;

        // 転置＋行列レイアウトの都合で入れ替えている
        out._11 = (float)m.m00; out._12 = (float)m.m10; out._13 = (float)m.m20; out._14 = 0.0f;
        out._21 = (float)m.m01; out._22 = (float)m.m11; out._23 = (float)m.m21; out._24 = 0.0f;
        out._31 = (float)m.m02; out._32 = (float)m.m12; out._33 = (float)m.m22; out._34 = 0.0f;
        out._41 = (float)m.m03; out._42 = (float)m.m13; out._43 = (float)m.m23; out._44 = 1.0f;

        return out;
    }

    // 位置ベクトルを行列 M で変換
    XMVECTOR TransformPosition(const XMFLOAT3& p, const XMMATRIX& M)
    {
        return XMVector3Transform(XMLoadFloat3(&p), M);
    }

    // 法線ベクトルを行列 M で変換（平行移動の影響は受けない）
    XMVECTOR TransformNormal(const XMFLOAT3& n, const XMMATRIX& M)
    {
        return XMVector3TransformNormal(XMLoadFloat3(&n), M);
    }

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
                    TransformPosition(bind_vertices[v].pos, B),
                    W, P);
                // N += (B * nrm) * w
                N = XMVectorMultiplyAdd(
                    TransformNormal(bind_vertices[v].nrm, B),
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
        XMFLOAT4X4     xm_local = ToXMMatrix(lm);
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

//================================================================
// コンストラクタ
// ・メンバはヘッダ側のデフォルト初期化に任せており、ここでは何もしない
//================================================================
UfbxStaticModel::UfbxStaticModel()
{
}

//================================================================
// シーン読み込み
// ・ufbx_load_file を呼び出すラッパ
// ・unique_ptr に ufbx_free_scene をセットして管理しやすくする
//================================================================
bool UfbxStaticModel::LoadScene(
    const char* fbx_path,
    std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)>& scene_out)
{
    ufbx_error     err;
    ufbx_load_opts opts;

    std::memset(&err, 0, sizeof(err));
    std::memset(&opts, 0, sizeof(opts));

    // 左手系 / Y-up / Z-forward に変換
    // → DirectX の座標系に合わせるための設定
    opts.target_axes = ufbx_axes_left_handed_y_up;
    opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;

    // FBX ファイル読み込み
    ufbx_scene* raw_scene = ufbx_load_file(fbx_path, &opts, &err);
    if (!raw_scene) {
        return false;
    }

    // unique_ptr に生ポインタとデリータを設定
    scene_out = std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)>(
        raw_scene, ufbx_free_scene);
    return true;
}

//================================================================
// Load（FBX 読み込みのメイン関数）
// ・このクラスを使う側は基本的にこれだけ呼べば良い
//================================================================
bool UfbxStaticModel::Load(const char* fbx_path)
{
    // ufbx_scene を unique_ptr で管理（最初は nullptr）
    std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)> scene(
        nullptr, ufbx_free_scene);

    // シーン読み込み
    if (!LoadScene(fbx_path, scene)) {
        return false;
    }

    // スケルトン（ボーン情報）構築
    if (!BuildSkeletonFromScene(scene.get())) {
        return false;
    }

    // メッシュ展開（頂点・インデックス生成）
    ExpandAllNodes(scene.get());

    // 頂点／インデックスの GPU バッファ作成
    if (!CreateGpuBuffers()) {
        return false;
    }

    // エフェクト＆テクスチャの作成
    if (!CreateEffectsAndTextures(fbx_path, scene.get())) {
        return false;
    }

    return true;
}

//================================================================
// スケルトン構築
// ・シーン中のスキン情報からボーンとそのバインドポーズを取り出す
//================================================================
bool UfbxStaticModel::BuildSkeletonFromScene(const ufbx_scene* scene)
{
    // いったん全部クリア
    skeleton_.bones_.clear();
    skeleton_.curr_world_.clear();
    skeleton_.bone_index_of_.clear();
    skeleton_.skin_mats_.clear();

    if (!scene) {
        return false;
    }

    // ufbx_node* → ボーンインデックス の一時マップ
    std::unordered_map<const ufbx_node*, int> index_of;

    // 全ノードをループ
    size_t ni;
    for (ni = 0; ni < scene->nodes.count; ++ni) {
        const ufbx_node* node = scene->nodes.data[ni];
        const ufbx_mesh* mesh = node->mesh;
        if (!mesh) continue; // メッシュを持たないノードはスキップ

        // メッシュについているスキンデフォーマを全て見る
        size_t si;
        for (si = 0; si < mesh->skin_deformers.count; ++si) {
            const ufbx_skin_deformer* skin =
                mesh->skin_deformers.data[si];

            // 各クラスター（ボーン＋頂点群）の情報を見る
            size_t ci;
            for (ci = 0; ci < skin->clusters.count; ++ci) {
                const ufbx_skin_cluster* cl =
                    skin->clusters.data[ci];
                if (!cl->bone_node) continue;

                // このクラスターのボーンを SkeletonData に登録
                int bi = AddBoneInternal(
                    cl->bone_node, skeleton_, index_of);

                // ボーン側のバインド姿勢（ボーンのワールド行列）
                skeleton_.bones_[bi].bind_world =
                    ToXMMatrix(cl->bind_to_world);

                // バインド姿勢の逆行列も作っておく
                {
                    XMMATRIX B =
                        XMLoadFloat4x4(&skeleton_.bones_[bi].bind_world);
                    XMMATRIX B_inv = XMMatrixInverse(nullptr, B);
                    XMStoreFloat4x4(
                        &skeleton_.bones_[bi].inv_bind_world, B_inv);
                }

                // ジオメトリ → ボーン の変換行列
                skeleton_.bones_[bi].geom_bind_world =
                    ToXMMatrix(cl->geometry_to_bone);

                // その逆行列
                {
                    XMMATRIX G =
                        XMLoadFloat4x4(&skeleton_.bones_[bi].geom_bind_world);
                    XMMATRIX G_inv = XMMatrixInverse(nullptr, G);
                    XMStoreFloat4x4(
                        &skeleton_.bones_[bi].inv_geom_bind_world, G_inv);
                }

                // 親ボーンとの関係（親があればインデックスを調べてセット）
                if (cl->bone_node->parent) {
                    std::unordered_map<const ufbx_node*, int>::iterator itp =
                        index_of.find(cl->bone_node->parent);
                    if (itp != index_of.end()) {
                        skeleton_.bones_[bi].parent = itp->second;
                    }
                }
            }
        }
    }

    // ボーンが 1 本も無ければ、そのまま true（メッシュだけのモデル対応）
    if (skeleton_.bones_.empty()) {
        return true;
    }

    // 現在姿勢・スキン行列用配列をボーン数に合わせて確保
    skeleton_.curr_world_.resize(skeleton_.bones_.size());
    skeleton_.skin_mats_.resize(skeleton_.bones_.size());

    // ufbx_node* → uint16_t ボーン番号 のマップを作る
    uint16_t i16 = 0;
    size_t i;
    for (i = 0; i < skeleton_.bones_.size(); ++i) {
        // 初期姿勢はバインド姿勢
        skeleton_.curr_world_[i] = skeleton_.bones_[i].bind_world;
        if (skeleton_.bones_[i].node && i < 0x10000) {
            i16 = static_cast<uint16_t>(i);
            skeleton_.bone_index_of_[skeleton_.bones_[i].node] = i16;
        }
    }

    return true;
}

//================================================================
// アニメ評価（オーバーロードその1）
// ・scene->anim（デフォルトのアニメ）を使う
//================================================================
void UfbxStaticModel::UpdateSkeletonAtTime(
    const ufbx_scene* scene,
    double            t_sec)
{
    const ufbx_anim* anim = scene ? scene->anim : nullptr;
    UpdateSkeletonAtTime(scene, anim, t_sec);
}

//================================================================
// アニメ評価（オーバーロードその2）
// ・アニメーションスタックをインデックスで選ぶ
//================================================================
void UfbxStaticModel::UpdateSkeletonAtTime(
    const ufbx_scene* scene,
    size_t            stack_index,
    double            t_sec)
{
    const ufbx_anim* anim = nullptr;

    if (scene && stack_index < scene->anim_stacks.count) {
        const ufbx_anim_stack* st =
            scene->anim_stacks.data[stack_index];
        if (st) {
            anim = st->anim;
        }
    }
    UpdateSkeletonAtTime(scene, anim, t_sec);
}

//================================================================
// アニメ評価（オーバーロードその3）
// ・アニメーションスタックを名前で選ぶ
//================================================================
void UfbxStaticModel::UpdateSkeletonAtTime(
    const ufbx_scene* scene,
    const std::string& stack_name,
    double             t_sec)
{
    const ufbx_anim* anim = nullptr;

    if (scene && !stack_name.empty()) {
        size_t i;
        for (i = 0; i < scene->anim_stacks.count; ++i) {
            const ufbx_anim_stack* st =
                scene->anim_stacks.data[i];
            if (!st || !st->name.data) continue;

            if (std::strcmp(st->name.data, stack_name.c_str()) == 0) {
                anim = st->anim;
                break;
            }
        }
    }
    UpdateSkeletonAtTime(scene, anim, t_sec);
}

//================================================================
// アニメ評価本体
// ・指定時刻 t_sec における各ボーンのワールド行列を計算し
//   skeleton_.curr_world_ に書き込む
//================================================================
void UfbxStaticModel::UpdateSkeletonAtTime(
    const ufbx_scene* scene,
    const ufbx_anim* anim,
    double            t_sec)
{
    if (!scene || !anim) return;
    if (skeleton_.bones_.empty()) return;

    // t をアニメーション時間の範囲にクランプ
    double t = t_sec;
    if (anim->time_end > anim->time_begin) {
        if (t < anim->time_begin) t = anim->time_begin;
        if (t > anim->time_end)   t = anim->time_end;
    }

    // 計算済みノードの結果をためるキャッシュ
    std::unordered_map<const ufbx_node*, DirectX::XMFLOAT4X4> cache;

    skeleton_.curr_world_.resize(skeleton_.bones_.size());

    size_t i;
    for (i = 0; i < skeleton_.bones_.size(); ++i) {
        const ufbx_node* node = skeleton_.bones_[i].node;
        if (!node) continue;

        // ボーンに対応するノードのワールド行列を求める
        skeleton_.curr_world_[i] =
            EvaluateNodeWorldRecursive(node, anim, t, cache);
    }
}

//================================================================
// メッシュ展開（CPU）
// ・FBX のデータ構造（faces / vertex_position / uv 等）から
//   自前の頂点配列・インデックス配列を組み立てる
//================================================================
void UfbxStaticModel::ExpandAllNodes(const ufbx_scene* scene)
{
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

    // シーン中の全ノードをチェック
    size_t ni;
    for (ni = 0; ni < scene->nodes.count; ++ni) {
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

            size_t v;
            for (v = 0; v < vtx_list.count; ++v) {
                const ufbx_skin_vertex sv = vtx_list.data[v];
                uint32_t begin = sv.weight_begin;
                uint32_t n = sv.num_weights;

                uint32_t k;
                for (k = 0; k < n; ++k) {
                    const ufbx_skin_weight w = w_list.data[begin + k];
                    uint32_t ci = w.cluster_index;
                    if (ci >= clusters.count) continue;

                    const ufbx_skin_cluster* cl =
                        clusters.data[ci];
                    if (!cl || !cl->bone_node) continue;

                    // このクラスターのボーンが SkeletonData にあるか調べる
                    std::unordered_map<const ufbx_node*, uint16_t>::iterator it =
                        skeleton_.bone_index_of_.find(cl->bone_node);
                    if (it == skeleton_.bone_index_of_.end()) {
                        continue;
                    }

                    acc[v].push_back(
                        std::make_pair(it->second, (float)w.weight));
                }
            }

            // 4 本までに絞って正規化
            size_t v2;
            for (v2 = 0; v2 < acc.size(); ++v2) {
                std::vector< std::pair<uint16_t, float> >& list = acc[v2];
                if (list.empty()) continue;

                // ウェイトの大きい順にソート
                std::sort(list.begin(), list.end(), CompareBoneWeightPair);

                // 最大 4 本だけ使う
                if (list.size() > 4) {
                    list.resize(4);
                }

                float sum = 0.0f;
                size_t i;
                for (i = 0; i < list.size(); ++i) {
                    sum += list[i].second;
                }
                if (sum <= 0.0f) sum = 1.0f;

                VertexInfluence vi;
                size_t n_infl = list.size();
                if (n_infl > 4) n_infl = 4;

                for (i = 0; i < n_infl; ++i) {
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

        uint32_t fi;
        for (fi = 0; fi < (uint32_t)mesh->faces.count; ++fi) {
            uint32_t mi =
                (mesh->face_material.count > 0) ?
                mesh->face_material.data[fi] : 0;
            faces_by_mat[mi].push_back(fi);
        }

        // 各マテリアルごとに MeshPart を作って頂点展開
        std::unordered_map<uint32_t, std::vector<uint32_t> >::iterator it_mat =
            faces_by_mat.begin();
        for (; it_mat != faces_by_mat.end(); ++it_mat) {
            uint32_t mat_index = it_mat->first;
            const std::vector<uint32_t>& face_list = it_mat->second;

            MeshPart part;
            // 一度 null で初期化しておき、あとで正しいマテリアルをセット
            //part.mat = GetDiffuseTexture(nullptr) ? nullptr : nullptr; // 初期値
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
            const ufbx_texture* tex_for_uv = GetDiffuseTexture(part.mat);
            const ufbx_vertex_vec2* uvv = base_uv;
            if (tex_for_uv && tex_for_uv->uv_set.length > 0) {
                const ufbx_vertex_vec2* alt =
                    ResolveUVByName(mesh, tex_for_uv->uv_set);
                if (alt && alt->exists) {
                    uvv = alt;
                }
            }

            // 登録されたフェイスを全て三角形に分解して頂点生成
            size_t face_idx;
            for (face_idx = 0; face_idx < face_list.size(); ++face_idx) {
                uint32_t f_index = face_list[face_idx];
                const ufbx_face f = mesh->faces.data[f_index];
                if (f.num_indices < 3) continue;

                // n角形を「扇形分割」で三角形にする
                uint32_t k;
                for (k = 0; k + 2 < f.num_indices; ++k) {
                    uint32_t corners[3];
                    corners[0] = f.index_begin + 0;
                    corners[1] = f.index_begin + (k + 1);
                    corners[2] = f.index_begin + (k + 2);

                    int c;
                    for (c = 0; c < 3; ++c) {
                        uint32_t corner = corners[c];
                        uint32_t vtx = mesh->vertex_indices.data[corner];

                        // 位置
                        uint32_t pi =
                            ValueIndexOf(mesh->vertex_position, corner, vtx);
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
                                ValueIndexOf(mesh->vertex_normal, corner, vtx);
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
                                ValueIndexOf(*uvv, corner, vtx);
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

    skeleton_.scene_radius_ = r;

    // バインドポーズ頂点・スキニング結果頂点を準備
    mesh_.bind_vertices_ = mesh_.vertices_;
    mesh_.skinned_vertices_ = mesh_.vertices_;
}

//================================================================
// GPU バッファ作成
// ・CPU 側の頂点配列／インデックス配列から VB / IB を作る
//================================================================
bool UfbxStaticModel::CreateGpuBuffers()
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
            &desc, &init, draw_.vb_.GetAddressOf());
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
            &desc, &init, draw_.ib_.GetAddressOf());
        if (FAILED(hr)) {
            return false;
        }
    }

    return true;
}

//================================================================
// エフェクト＆テクスチャ作成
// ・BasicEffect / CommonStates の初期化
// ・各 MeshPart のテクスチャ(SRV) を作成
//================================================================
bool UfbxStaticModel::CreateEffectsAndTextures(
    const char* fbx_path,
    const ufbx_scene* scene)
{
    ID3D11Device* device = Gfx::Dev();
    ID3D11DeviceContext* ctx = Gfx::Ctx();
    if (!device || !ctx) return false;

    // CommonStates / BasicEffect の確保
    if (!draw_.states_) {
        draw_.states_.reset(new DirectX::CommonStates(device));
    }
    if (!draw_.fx_) {
        draw_.fx_.reset(new DirectX::BasicEffect(device));
    }

    DirectX::BasicEffect* fx = draw_.fx_.get();

    // 簡単なライティング設定
    fx->SetLightingEnabled(true);
    fx->SetPerPixelLighting(true);
    fx->SetVertexColorEnabled(false);
    fx->SetTextureEnabled(true);
    fx->SetAmbientLightColor({ 0.25f, 0.25f, 0.30f });
    fx->SetDiffuseColor({ 0.90f, 0.90f, 0.95f, 1.0f });
    fx->SetLightEnabled(0, true);
    fx->SetLightDirection(0, { -0.5f, -1.0f, -0.3f });
    fx->SetLightDiffuseColor(0, { 1.0f, 1.0f, 1.0f, 1.0f });

    // 入力レイアウト（頂点の並び方の説明）
    if (!draw_.layout_) {
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
            il, 3, bc, sz, draw_.layout_.GetAddressOf());
        if (FAILED(hr)) {
            return false;
        }
    }

    // テクスチャロード用に FBX ファイルのディレクトリを覚えておく
    fs::path fbx_dir;
    if (fbx_path) {
        size_t len = std::strlen(fbx_path);
        fbx_dir = PathFromUtf8(fbx_path, len).parent_path();
    }

    // 各 MeshPart のテクスチャを作成
    size_t i;
    for (i = 0; i < mesh_.parts_.size(); ++i) {
        MeshPart& part = mesh_.parts_[i];

        const ufbx_texture* tex = GetDiffuseTexture(part.mat);
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
            fs::path tex_path = fbx_dir / FileNameFromUfbx(tex->filename);
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
// ・必要なら CPU スキニングを行い、VB を更新してから描画
//================================================================
void UfbxStaticModel::Draw(
    const DirectX::XMMATRIX& world,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& proj)
{
    ID3D11DeviceContext* ctx = Gfx::Ctx();
    if (!ctx) return;

    if (!draw_.vb_ || !draw_.ib_) return;
    if (!draw_.fx_ || !draw_.states_ || !draw_.layout_) return;
    if (mesh_.indices_.empty()) return;

    using namespace DirectX;

    // CPU スキニング（ボーン情報とウェイトがある場合のみ）
    if (!mesh_.influences_.empty() && !mesh_.bind_vertices_.empty()) {
        skeleton_.skin_mats_.resize(skeleton_.bones_.size());

        size_t i;
        for (i = 0; i < skeleton_.bones_.size(); ++i) {
            XMMATRIX W = XMLoadFloat4x4(&skeleton_.curr_world_[i]);
            XMMATRIX G2B =
                XMLoadFloat4x4(&skeleton_.bones_[i].geom_bind_world);
            // スキン行列 = 現在ボーン姿勢 × ジオメトリ→ボーン
            skeleton_.skin_mats_[i] = XMMatrixMultiply(G2B, W);
        }

        // CPU でスキニングして頂点を更新
        ApplySkinCPU(
            skeleton_.skin_mats_,
            mesh_.influences_,
            mesh_.bind_vertices_,
            mesh_.skinned_vertices_);

        // GPU の頂点バッファにスキニング結果を書き戻す
        ctx->UpdateSubresource(
            draw_.vb_.Get(), 0, nullptr,
            &mesh_.skinned_vertices_[0], 0, 0);
    }

    // IA ステージ設定
    UINT stride = sizeof(VertexPNT2);
    UINT offset = 0;
    ID3D11Buffer* vb = draw_.vb_.Get();

    ctx->IASetInputLayout(draw_.layout_.Get());
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(draw_.ib_.Get(), DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 深度ステンシル・ラスタライザ・サンプラ設定
    ctx->OMSetDepthStencilState(draw_.states_->DepthDefault(), 0);
    ctx->RSSetState(draw_.states_->CullCounterClockwise());

    ID3D11SamplerState* samp = draw_.states_->LinearClamp();
    ctx->PSSetSamplers(0, 1, &samp);

    // 行列設定
    draw_.fx_->SetWorld(world);
    draw_.fx_->SetView(view);
    draw_.fx_->SetProjection(proj);

    // 現在のブレンドステートを退避
    ID3D11BlendState* prev_blend = nullptr;
    FLOAT             prev_factor[4];
    UINT              prev_mask = 0xFFFFFFFF;
    ctx->OMGetBlendState(&prev_blend, prev_factor, &prev_mask);

    ID3D11BlendState* bound = prev_blend;

    // 各 MeshPart を描画
    size_t i_part;
    for (i_part = 0; i_part < mesh_.parts_.size(); ++i_part) {
        const MeshPart& part = mesh_.parts_[i_part];

        bool has_tex = (part.srv != nullptr);
        // テクスチャありなら αブレンド（NonPremultiplied）、なければ不透明
        ID3D11BlendState* target =
            has_tex ? draw_.states_->NonPremultiplied()
            : draw_.states_->Opaque();

        if (bound != target) {
            ctx->OMSetBlendState(target, nullptr, 0xFFFFFFFF);
            bound = target;
        }

        draw_.fx_->SetTextureEnabled(has_tex);
        if (has_tex) {
            draw_.fx_->SetTexture(part.srv.Get());
        }

        draw_.fx_->Apply(ctx);
        ctx->DrawIndexed(part.index_count, part.start_index, 0);
    }

    // ブレンドステートを元に戻す
    if (prev_blend) {
        ctx->OMSetBlendState(prev_blend, prev_factor, prev_mask);
        prev_blend->Release();
    }
}

//================================================================
// 骨デバッグ描画
// ・ボーン同士を線で結んで表示
// ・各ボーンのローカル軸も x/y/z 別の色で表示
//================================================================
void UfbxStaticModel::DrawSkeleton(
    const DirectX::XMMATRIX& world,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& proj)
{
    using namespace DirectX;

    ID3D11DeviceContext* ctx = Gfx::Ctx();
    if (!ctx) return;

    if (skeleton_.bones_.empty()) return;

    // デバッグ描画用のリソースを初期化（初回のみ）
    if (!draw_.debug_fx_) {
        ID3D11Device* dev = Gfx::Dev();
        if (!dev) return;

        draw_.debug_fx_.reset(new BasicEffect(dev));
        draw_.debug_fx_->SetVertexColorEnabled(true);
        draw_.debug_fx_->SetLightingEnabled(false);

        const void* bc = nullptr;
        size_t      sz = 0;
        draw_.debug_fx_->GetVertexShaderBytecode(&bc, &sz);

        D3D11_INPUT_ELEMENT_DESC il[] =
        {
            { "SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,
              0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",       0, DXGI_FORMAT_R32G32B32A32_FLOAT,
              0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        dev->CreateInputLayout(
            il, 2, bc, sz, draw_.debug_layout_.GetAddressOf());

        D3D11_BUFFER_DESC bd;
        std::memset(&bd, 0, sizeof(bd));
        bd.ByteWidth = sizeof(DebugVC) * 4096;
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        dev->CreateBuffer(&bd, nullptr, draw_.bone_vb_.GetAddressOf());
        draw_.bone_vb_size_ = bd.ByteWidth / sizeof(DebugVC);
    }

    // ライン頂点をためるバッファ
    std::vector<DebugVC> lines;
    lines.reserve(skeleton_.bones_.size() * 8);

    XMFLOAT4 col_bone(1.0f, 0.9f, 0.2f, 1.0f);
    XMFLOAT4 col_x(1, 0, 0, 1);
    XMFLOAT4 col_y(0, 1, 0, 1);
    XMFLOAT4 col_z(0, 0.5f, 1, 1);

    // ボーン軸の長さ（モデルのサイズに合わせて調整）
    float axis_len = skeleton_.scene_radius_ * 0.03f;
    if (axis_len < 0.02f) axis_len = 0.02f;

    // 各ボーンについてラインを登録
    size_t i;
    for (i = 0; i < skeleton_.bones_.size(); ++i) {
        int parent = skeleton_.bones_[i].parent;
        const XMFLOAT4X4& Wi = skeleton_.curr_world_[i];

        // 親がいれば、親→子のラインを引く
        if (parent >= 0) {
            XMFLOAT3 p0 = GetBonePosition(
                skeleton_.curr_world_[parent]);
            XMFLOAT3 p1 = GetBonePosition(Wi);
            lines.push_back(DebugVC{ p0, col_bone });
            lines.push_back(DebugVC{ p1, col_bone });
        }

        // ボーンの軸（X,Y,Z）を描く
        XMFLOAT3 o = GetBonePosition(Wi);
        XMFLOAT3 x1 = TransformPoint(
            Wi, XMFLOAT3(axis_len, 0, 0));
        XMFLOAT3 y1 = TransformPoint(
            Wi, XMFLOAT3(0, axis_len, 0));
        XMFLOAT3 z1 = TransformPoint(
            Wi, XMFLOAT3(0, 0, axis_len));

        lines.push_back(DebugVC{ o,  col_x }); lines.push_back(DebugVC{ x1, col_x });
        lines.push_back(DebugVC{ o,  col_y }); lines.push_back(DebugVC{ y1, col_y });
        lines.push_back(DebugVC{ o,  col_z }); lines.push_back(DebugVC{ z1, col_z });
    }

    if (lines.empty()) return;

    // 必要ならボーン用 VB を作り直す（頂点数が足りないとき）
    if (draw_.bone_vb_size_ < lines.size()) {
        draw_.bone_vb_.Reset();

        ID3D11Device* dev = Gfx::Dev();
        if (!dev) return;

        D3D11_BUFFER_DESC bd;
        std::memset(&bd, 0, sizeof(bd));
        bd.ByteWidth = (UINT)(lines.size() * sizeof(DebugVC));
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        dev->CreateBuffer(&bd, nullptr, draw_.bone_vb_.GetAddressOf());
        draw_.bone_vb_size_ = lines.size();
    }

    // VB にラインデータを書き込む
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(ctx->Map(
        draw_.bone_vb_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        std::memcpy(mapped.pData,
            &lines[0], lines.size() * sizeof(DebugVC));
        ctx->Unmap(draw_.bone_vb_.Get(), 0);
    }

    // IA 設定してライン描画
    UINT stride = sizeof(DebugVC);
    UINT offset = 0;
    ID3D11Buffer* vb = draw_.bone_vb_.Get();

    ctx->IASetInputLayout(draw_.debug_layout_.Get());
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    draw_.debug_fx_->SetWorld(world);
    draw_.debug_fx_->SetView(view);
    draw_.debug_fx_->SetProjection(proj);
    draw_.debug_fx_->Apply(ctx);

    ctx->Draw((UINT)lines.size(), 0);
}
