#pragma once

#include <DirectXMath.h>
#include <wrl/client.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include <Effects.h>
#include <CommonStates.h>

// ufbx を前方宣言（ヘッダに直接依存しないようにする）
struct ufbx_scene;
struct ufbx_node;
struct ufbx_material;
struct ufbx_anim;

// Direct3D のリソースも前方宣言
struct ID3D11Buffer;
struct ID3D11ShaderResourceView;
struct ID3D11InputLayout;

// ============================================================================
// FBX（ufbx）から読み込んだ「メッシュ＋スケルトン（ボーン）」をまとめて扱うクラス
// ・FBX を読み込んでメッシュ（頂点＋インデックス）を作る
// ・ボーン情報（スケルトン）を作る
// ・Draw() でメッシュを描画
// ・DrawSkeleton() でボーンのラインを描画（デバッグ用）
// DirectX の細かい処理（頂点バッファやエフェクト）は内部で隠す設計
// ============================================================================
class UfbxStaticModel
{
public:
    // ------------------------------------------------------------
    // 頂点構造体（Position / Normal / Texcoord）
    // DirectXTK の BasicEffect でそのまま使える形
    // ------------------------------------------------------------
    struct VertexPNT2
    {
        DirectX::XMFLOAT3 pos;  // 頂点位置（x, y, z）
        DirectX::XMFLOAT3 nrm;  // 法線ベクトル（ライティング用）
        DirectX::XMFLOAT2 uv;   // UV 座標（テクスチャ参照用）
    };

    // ------------------------------------------------------------
    // 1頂点が影響を受けるボーン情報
    // ・最大4つのボーン番号と、そのウェイトを持つ
    // ・スキニング（頂点をボーンに合わせて動かす）に使う
    // ------------------------------------------------------------
    struct VertexInfluence
    {
        uint16_t bone[4]{};   // 影響を与えるボーンのインデックス（0〜3）
        float    weight[4]{}; // 各ボーンの重み（合計 ≒ 1.0）
    };

    // ------------------------------------------------------------
    // メッシュの一部分（サブメッシュ）の情報
    // ・1つのマテリアルに対応する三角形のまとまり
    // ・同じテクスチャで描ける範囲ごとに 1 MeshPart
    // ------------------------------------------------------------
    struct MeshPart
    {
        // このパートに使う ufbx のマテリアル
        const ufbx_material* mat = nullptr;

        // インデックスバッファの中で、このパートが始まる位置
        uint32_t             start_index = 0;

        // このパートで描画するインデックス数
        uint32_t             index_count = 0;

        // このパートに使うテクスチャ（SRV）
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    };

    // ------------------------------------------------------------
    // ボーン1本分の情報
    // ・どの ufbx_node に対応しているか
    // ・親ボーンのインデックス
    // ・バインドポーズ（初期姿勢）の行列
    // ------------------------------------------------------------
    struct BoneInfo
    {
        const ufbx_node* node = nullptr; // このボーンに対応する ufbx のノード
        int              parent = -1;    // 親ボーンのインデックス（なければ -1）

        DirectX::XMFLOAT4X4 bind_world{};       // ボーンのバインド姿勢のワールド行列
        DirectX::XMFLOAT4X4 inv_bind_world{};   // 上の逆行列
        DirectX::XMFLOAT4X4 geom_bind_world{};  // ジオメトリ → ボーン の変換行列
        DirectX::XMFLOAT4X4 inv_geom_bind_world{}; // その逆行列
    };

    // ------------------------------------------------------------
    // スケルトン全体のデータ
    // ・ボーン配列
    // ・現在のボーンのワールド行列（アニメーションで変化）
    // ・u fbx のノード → ボーン番号へのマップ
    // ・スキニング用の行列キャッシュ
    // ------------------------------------------------------------
    struct SkeletonData
    {
        // 全ボーンの情報（配列のインデックス = ボーン番号）
        std::vector<BoneInfo>                      bones_;

        // 現在時刻 t におけるボーンのワールド行列（node_to_world(t)）
        std::vector<DirectX::XMFLOAT4X4>           curr_world_;

        // ufbx_node* からボーン番号（uint16_t）を引くための辞書
        std::unordered_map<const ufbx_node*, uint16_t> bone_index_of_;

        // CPU スキニングで使うスキン行列の配列（毎フレーム更新）
        std::vector<DirectX::XMMATRIX>             skin_mats_;

        // シーンのおおよその大きさ（半径）
        // → ボーンのデバッグ描画で軸の長さを決めるために使う
        float                                      scene_radius_ = 1.0f;
    };

    // ------------------------------------------------------------
    // コンストラクタ（ここでは実体は作らない、初期値だけ）
    // ------------------------------------------------------------
    UfbxStaticModel();

    // ------------------------------------------------------------
    // FBX 読み込み
    // ・fbx_path から FBX を読み込み
    // ・メッシュ展開、スケルトン構築、GPUバッファ作成、テクスチャ読み込みまでやる
    // ・成功したら true、失敗したら false
    // ------------------------------------------------------------
    bool Load(const char* fbx_path);

    // ------------------------------------------------------------
    // メッシュ描画
    // ・world, view, proj 行列を渡して描画
    // ・内部で CPU スキニング → 頂点バッファ更新 → 各 MeshPart を描画
    // ------------------------------------------------------------
    void Draw(const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj);

    // ------------------------------------------------------------
    // スケルトン（ボーン）のデバッグ描画
    // ・ボーン同士を線で結んで表示
    // ・各ボーンのローカル座標軸も表示（xyz で色分け）
    // ------------------------------------------------------------
    void DrawSkeleton(const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj);

    // ------------------------------------------------------------
    // スケルトン構築／アニメーション用インターフェース
    // ------------------------------------------------------------

    // シーンからボーン情報を抽出して SkeletonData を構築
    bool BuildSkeletonFromScene(const ufbx_scene* scene);

    // デフォルトのアニメ（scene->anim）で時刻 t_sec の姿勢に更新
    void UpdateSkeletonAtTime(const ufbx_scene* scene, double t_sec);

    // 明示的に anim を指定して時刻 t_sec の姿勢に更新
    void UpdateSkeletonAtTime(const ufbx_scene* scene, const ufbx_anim* anim, double t_sec);

    // アニメーションスタックのインデックス指定で更新
    void UpdateSkeletonAtTime(const ufbx_scene* scene, size_t stack_index, double t_sec);

    // アニメーションスタック名指定で更新
    void UpdateSkeletonAtTime(const ufbx_scene* scene, const std::string& stack_name, double t_sec);

    // ------------------------------------------------------------
    // FBX シーン読み込み（ufbx のラッパ）
    // ・呼び出し側は unique_ptr<ufbx_scene> を受け取れる
    // ------------------------------------------------------------
    bool LoadScene(const char* fbx_path,
        std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)>& scene_out);

private:

    // ------------------------------------------------------------
    // メッシュ全体の CPU 側データ
    // ・頂点配列
    // ・インデックス配列
    // ・MeshPart（サブメッシュ）配列
    // ・スキニング用の補助データ
    // ------------------------------------------------------------
    struct MeshData
    {
        // 描画元となる頂点データ（スキニング後に更新される）
        std::vector<VertexPNT2>      vertices_;

        // インデックスデータ（三角形リスト）
        std::vector<uint32_t>        indices_;

        // サブメッシュ（マテリアルごと）情報
        std::vector<MeshPart>        parts_;

        // 頂点ごとのボーンインフルエンス
        std::vector<VertexInfluence> influences_;

        // バインドポーズ時の頂点（＝スキニング前の元データ）
        std::vector<VertexPNT2>      bind_vertices_;

        // スキニング結果を入れる頂点配列
        std::vector<VertexPNT2>      skinned_vertices_;
    };

    // ------------------------------------------------------------
    // 描画処理に必要な DirectX リソース一式
    // ------------------------------------------------------------
    struct DrawResources
    {
        // メッシュ描画用
        Microsoft::WRL::ComPtr<ID3D11Buffer>       vb_;      // 頂点バッファ
        Microsoft::WRL::ComPtr<ID3D11Buffer>       ib_;      // インデックスバッファ
        Microsoft::WRL::ComPtr<ID3D11InputLayout>  layout_;  // 入力レイアウト
        std::unique_ptr<DirectX::CommonStates>     states_;  // 汎用ステート（ブレンド、サンプラ等）
        std::unique_ptr<DirectX::BasicEffect>      fx_;      // BasicEffect（ライティング＋テクスチャ）

        // ボーンデバッグ描画用
        std::unique_ptr<DirectX::BasicEffect>      debug_fx_;   // 頂点カラーのみのエフェクト
        Microsoft::WRL::ComPtr<ID3D11InputLayout>  debug_layout_; // デバッグライン用レイアウト
        Microsoft::WRL::ComPtr<ID3D11Buffer>       bone_vb_;    // ボーン線描画用頂点バッファ
        size_t                                     bone_vb_size_ = 0; // バッファに確保されている頂点数
    };

    // ------------------------------------------------------------
    // 内部処理（.cpp で実装）
    // ------------------------------------------------------------

    // シーン中の全ノードを走査してメッシュを展開（頂点・インデックスを作成）
    void ExpandAllNodes(const ufbx_scene* scene);

    // GPU の頂点バッファ／インデックスバッファを作成
    bool CreateGpuBuffers();

    // BasicEffect やテクスチャ（SRV）を作成
    bool CreateEffectsAndTextures(const char* fbx_path,
        const ufbx_scene* scene);

    // ------------------------------------------------------------
    // インスタンスが持つデータ
    // ------------------------------------------------------------
    SkeletonData skeleton_;   // ボーン関連の全情報
    MeshData     mesh_;       // メッシュ関連のCPU側データ
    DrawResources draw_;      // 描画に必要な DirectX リソース群
};
