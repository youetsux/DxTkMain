#pragma once

#include <memory>
#include <string>

#include <DirectXMath.h>
#include "ufbx.h"
#include "FbxSkeleton.h"
#include "FbxMesh.h"
#include "FbxMeshGroup.h"
#include "BoundingVolume.h"


enum class SizeMeasureAxis
{
    HeightY,      // max.y - min.y
    WidthX,       // max.x - min.x
    DepthZ,       // max.z - min.z
    MaxExtent,    // max(x,y,z)
    Radius,       // 球の半径
};


// ufbx 前宣言（ヘッダに直接定義を持ち込まないように）
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

    // FBX ファイル読み込み、スケルトン・メッシュ構築
    bool Load(const char* fbx_path);

    // 任意のリセット（再利用する場合など）
    void Reset();

    // ------------------------------------------------------------
    // 描画
    // ------------------------------------------------------------

    // メッシュ描画（CPU スキニング込み）
    void Draw(const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj);

    // スケルトンのデバッグ描画（ボーン軸など）
    void DrawSkeleton(const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj);

    // ------------------------------------------------------------
    // アニメーション
    // ------------------------------------------------------------

    // シーンに含まれる「デフォルトアニメ」を返す（なければ nullptr）
    const ufbx_anim* GetDefaultAnim() const;

    // デフォルトアニメの時刻 t_sec でスケルトン更新
    void UpdateSkeletonAtTime(double t_sec);

    // 任意の anim を指定して時刻 t_sec の姿勢に更新
    void UpdateSkeletonAtTime(const ufbx_anim* anim, double t_sec);

    // ------------------------------------------------------------
    // アクセサ
    // ------------------------------------------------------------
    const ufbx_scene* Scene()       const { return scene_.get(); }
    FbxSkeleton& Skeleton() { return skeleton_; }
    const FbxSkeleton& Skeleton()   const { return skeleton_; }
    FbxMesh& Mesh() { return mesh_; }
    const FbxMesh& Mesh()        const { return mesh_; }

    // 境界ボリューム アクセサ（Mesh にフォワード）
    BVolume& GetBV() { return mesh_.GetBV(); }
    const BVolume& GetBV() const { return mesh_.GetBV(); }

    // シーン半径アクセサ（Skeleton にフォワード）
    float SceneRadius() const { return skeleton_.SceneRadius(); }

    // マルチメッシュ対応用
    FbxMeshGroup& MeshGroup() { return mesh_group_; }
    const FbxMeshGroup& MeshGroup() const { return mesh_group_; }

private:
    // シーン読み込みの内部処理
    bool LoadScene(const char* fbx_path);

private:
    // ufbx シーン本体（FbxSkeleton / FbxMesh は参照として構築）
    std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)> scene_{ nullptr, ufbx_free_scene };

    // ボーン・アニメーション
    FbxSkeleton skeleton_;

    // メッシュ・テクスチャ・描画（単一メッシュの旧実装）
    FbxMesh     mesh_;

    // ノード/メッシュ配列（マルチメッシュ対応用）
    FbxMeshGroup mesh_group_;
};