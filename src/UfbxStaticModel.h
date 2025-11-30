#pragma once

#include <DirectXMath.h>
#include <wrl/client.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include <Effects.h>
#include <CommonStates.h>
#include "ufbx.h"
#include "FbxSkeleton.h"
#include "FbxMesh.h"

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

    // 公開ラッパ：内部の scene_ を使ってデフォルトアニメ（scene->anim）で更新
    void UpdateSkeletonAtTime(double t_sec);

    // 公開アクセサ：内部シーンのデフォルト anim（nullptr 可能）を取得
    const ufbx_anim* GetDefaultAnim() const;


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
        Microsoft::WRL::ComPtr<ID3D11Buffer>       vb_;
        Microsoft::WRL::ComPtr<ID3D11Buffer>       ib_;
        Microsoft::WRL::ComPtr<ID3D11InputLayout>  layout_;

        // ★ DX11 名前空間に統一
        std::unique_ptr<DirectX::DX11::CommonStates> states_;
        std::unique_ptr<DirectX::DX11::BasicEffect>  fx_;

        std::unique_ptr<DirectX::DX11::BasicEffect>  debug_fx_;
        Microsoft::WRL::ComPtr<ID3D11InputLayout>    debug_layout_;
        Microsoft::WRL::ComPtr<ID3D11Buffer>         bone_vb_;
        size_t                                       bone_vb_size_ = 0;
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
    FbxSkeleton skeleton_;   // ボーン関連の全情報
    FbxMesh mesh_;
    MeshData     meshData_;       // メッシュ関連のCPU側データ
    DrawResources draw_;      // 描画に必要な DirectX リソース群
    std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)> scene_{ nullptr, ufbx_free_scene };
};
