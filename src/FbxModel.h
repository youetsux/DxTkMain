#pragma once
#pragma once

#include <memory>
#include <string>

#include <DirectXMath.h>
#include "ufbx.h"
#include "FbxSkeleton.h"
#include "FbxMesh.h"
#include "BoundingVolume.h"


enum class SizeMeasureAxis
{
    HeightY,      // max.y - min.y
    WidthX,       // max.x - min.x
    DepthZ,       // max.z - min.z
    MaxExtent,    // max(x,y,z)
    Radius,       // 従来互換（必要なら）
};


// ufbx を前方宣言（ヘッダに直接依存しないようにする）
struct ufbx_scene;
struct ufbx_anim;

//======================================================================
// FbxModel
//   - ufbx_scene の所有
//   - FbxSkeleton（ボーン・アニメーション）
//   - FbxMesh（メッシュ・テクスチャ・描画）
// をまとめて扱うクラス
//======================================================================
class FbxModel
{
public:
    FbxModel();
    ~FbxModel();

    // ------------------------------------------------------------
    // 読み込み・破棄
    // ------------------------------------------------------------

    // FBX ファイルを読み込んで、スケルトン＋メッシュを構築
    bool Load(const char* fbx_path);

    // 明示的なリセット（再利用したい場合など）
    void Reset();

    // ------------------------------------------------------------
    // 描画
    // ------------------------------------------------------------

    // メッシュ描画（CPU スキニング込み）
    void Draw(const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj);

    // スケルトンのデバッグ描画（ボーンラインなど）
    void DrawSkeleton(const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj);

    // ------------------------------------------------------------
    // アニメーション制御
    // ------------------------------------------------------------

    // シーンに含まれる「デフォルトアニメ」を返す（なければ nullptr）
    const ufbx_anim* GetDefaultAnim() const;

    // デフォルトアニメの時刻 t_sec でスケルトンを更新
    void UpdateSkeletonAtTime(double t_sec);

    // 明示的に anim を指定して時刻 t_sec の姿勢に更新
    void UpdateSkeletonAtTime(const ufbx_anim* anim, double t_sec);

    // ------------------------------------------------------------
    // アクセサ
    // ------------------------------------------------------------
    const ufbx_scene* Scene()       const { return scene_.get(); }
    FbxSkeleton& Skeleton() { return skeleton_; }
    const FbxSkeleton& Skeleton()   const { return skeleton_; }
    FbxMesh& Mesh() { return mesh_; }
    const FbxMesh& Mesh()        const { return mesh_; }

    // ★ BV アクセサ（Mesh にフォワード）
    BVolume& GetBV() { return mesh_.GetBV(); }
    const BVolume& GetBV() const { return mesh_.GetBV(); }

    // ★ シーン半径アクセサ（Skeleton にフォワード）
    float SceneRadius();
    float SceneHeight();   // ★ 追加：Y 高さ（maxY - minY）
    float MeasureSize(SizeMeasureAxis axis);
    float MeasureSkinnedHeightY();

private:
    // シーン読み込みの下請け
    bool LoadScene(const char* fbx_path);
    
private:
    // ufbx シーン本体（FbxSkeleton / FbxMesh はこれを参照して構築する）
    std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)> scene_{ nullptr, ufbx_free_scene };

    // ボーン・アニメーション情報
    FbxSkeleton skeleton_;

    // メッシュ＋テクスチャ＋描画情報
    FbxMesh     mesh_;


};

