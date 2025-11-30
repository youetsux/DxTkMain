#include "Model.h"
#include "Gfx.h"
#include "ufbx.h"      // ufbx_anim を使うので追加
#include <algorithm>
#include <vector>
#include <cmath>
#include <cstring>


using namespace DirectX;

// ================================================================
// 無名 namespace（ファイル限定）
// ================================================================
namespace
{
    // ------------------------------------------------------------
    // ★ 定数（ALL_CAPS）
    // ------------------------------------------------------------

    constexpr float  MODEL_ANIMATION_FPS = 60.0f;
    constexpr size_t MODEL_DEFAULT_MAX_COUNT = 32;

    // ------------------------------------------------------------
    // ★ 内部ステート
    // ------------------------------------------------------------

    XMMATRIX g_viewMatrix = XMMatrixIdentity();
    XMMATRIX g_projMatrix = XMMatrixIdentity();
    bool     g_hasViewProj = false;

    std::vector<Model::ModelData> g_models;
    bool g_initialized = false;
}

// ================================================================
// モジュール初期化
// ================================================================
void Model::Initialize(size_t maxCount)
{
    g_models.clear();
    g_models.resize(maxCount);

    g_initialized = true;
}

// ================================================================
// 全破棄
// ================================================================
void Model::AllRelease()
{
    if (!g_initialized) return;

    for (auto& m : g_models)
    {
        m.used = false;
        m.ufbx.reset();
    }
}

// ================================================================
// 単体削除
// ================================================================
void Model::Delete(int handle)
{
    if (handle < 0 || handle >= (int)g_models.size()) return;

    ModelData& m = g_models[handle];

    m.used = false;
    m.ufbx.reset();
}

// ================================================================
// Transform 設定
// ================================================================
void Model::SetTransform(int handle, const Transform& transform)
{
    if (handle < 0 || handle >= (int)g_models.size()) return;
    g_models[handle].transform = transform;
}

// ================================================================
// Transform → 行列
// ================================================================
XMMATRIX Model::GetMatrix(int handle)
{
    if (handle < 0 || handle >= (int)g_models.size())
        return XMMatrixIdentity();

    return g_models[handle].transform.GetWorldMatrix();
}

// ================================================================
// UFBX モデル読み込み
// ================================================================
int Model::LoadUfbx(const char* fbxPath)
{
    if (!g_initialized)
        Initialize(MODEL_DEFAULT_MAX_COUNT);

    for (size_t i = 0; i < g_models.size(); ++i)
    {
        ModelData& m = g_models[i];
        if (m.used) continue;

        m.used = true;
        m.fileName = fbxPath;

        // ------------------------------
        // UfbxStaticModel だけでシーンを読み込む
        // ------------------------------
        m.ufbx = std::make_unique<UfbxStaticModel>();

        if (!m.ufbx->Load(fbxPath))
        {
            m.used = false;
            m.ufbx.reset();
            return -1;
        }

        // Transform を初期化（スケール1・回転0・位置0）
        m.transform = Transform();

        return static_cast<int>(i); // ← モデル ID (handle)
    }

    return -1;
}

// ================================================================
// UFBX 単体描画（Transform 内部使用）
// ================================================================
void Model::DrawUfbx(int handle)
{
    if (!g_initialized || !g_hasViewProj) return;
    if (handle < 0 || handle >= (int)g_models.size()) return;

    ModelData& m = g_models[handle];
    if (!m.used || !m.ufbx) return;

    DirectX::XMMATRIX world = m.transform.GetWorldMatrix();

    m.ufbx->Draw(world, g_viewMatrix, g_projMatrix);
}

// ================================================================
// Transform を指定して描画
// ================================================================
void Model::DrawUfbx(int handle, const Transform& transform)
{
    if (!g_initialized || !g_hasViewProj) return;
    if (handle < 0 || handle >= (int)g_models.size()) return;

    ModelData& m = g_models[handle];
    if (!m.used || !m.ufbx) return;

    DirectX::XMMATRIX world = transform.GetWorldMatrix();
    m.ufbx->Draw(world, g_viewMatrix, g_projMatrix);
}

// ================================================================
// UFBX 全体描画（view / proj を保存）
// ================================================================
void Model::DrawUfbxAll(const XMMATRIX& view, const XMMATRIX& proj)
{
    g_viewMatrix = view;
    g_projMatrix = proj;
    g_hasViewProj = true;

    for (size_t i = 0; i < g_models.size(); ++i)
    {
        if (g_models[i].used)
            DrawUfbx(static_cast<int>(i));
    }
}

//-------------------------------------------------------------
// 単体描画（Transform 指定・フレーム指定）
//-------------------------------------------------------------
void Model::DrawUfbx(int handle, const Transform& transform, int frame)
{
    if (!g_initialized || !g_hasViewProj) return;
    if (handle < 0) return;
    if (static_cast<size_t>(handle) >= g_models.size()) return;

    Model::ModelData& m = g_models[handle];
    if (!m.used || !m.ufbx) return;

    // Transform → ワールド行列
    DirectX::XMMATRIX world = transform.GetWorldMatrix();

    // UfbxStaticModel 内部のデフォルトアニメを取得
    const ufbx_anim* anim = m.ufbx->GetDefaultAnim();
    

    if (anim)
    {
        const double fps = MODEL_ANIMATION_FPS; // 60.0f
        const double frameSec = static_cast<double>(frame) / fps;
        double       t_sec = anim->time_begin + frameSec;

        // Clamp
        if (anim->time_end > anim->time_begin)
        {
            if (t_sec < anim->time_begin) t_sec = anim->time_begin;
            if (t_sec > anim->time_end)   t_sec = anim->time_end;
        }

        // 内部シーンを使ってスケルトンを更新するラッパを呼ぶ
        m.ufbx->UpdateSkeletonAtTime(t_sec);
    }

    // 描画
    m.ufbx->Draw(world, g_viewMatrix, g_projMatrix);
}



void Model::DrawUfbx(int handle, int frame)
{
    if (!g_initialized || !g_hasViewProj) return;
    if (handle < 0 || handle >= (int)g_models.size()) return;

    auto& m = g_models[handle];
    if (!m.used || !m.ufbx) return;

    DrawUfbx(handle, m.transform, frame);
}

void Model::DrawUfbx(int handle, const Transform& transform, double timeSec)
{
    if (!g_initialized || !g_hasViewProj) return;
    if (handle < 0) return;
    if (static_cast<size_t>(handle) >= g_models.size()) return;

    Model::ModelData& m = g_models[handle];
    if (!m.used || !m.ufbx) return;

    // Transform → ワールド行列
    DirectX::XMMATRIX world = transform.GetWorldMatrix();

    // デフォルトアニメを取得
    const ufbx_anim* anim = m.ufbx->GetDefaultAnim();
    if (anim)
    {
        // timeSec は「0 からの経過時間 [秒]」という想定
        double t_sec = anim->time_begin + timeSec;

        // アニメ時間範囲内にクランプ
        if (anim->time_end > anim->time_begin)
        {
            if (t_sec < anim->time_begin) t_sec = anim->time_begin;
            if (t_sec > anim->time_end)   t_sec = anim->time_end;
        }

        // 指定時刻のポーズにスケルトンを更新
        m.ufbx->UpdateSkeletonAtTime(t_sec);
    }

    // 描画
    m.ufbx->Draw(world, g_viewMatrix, g_projMatrix);
}

// ================================================================
// スケルトン全体描画
// ================================================================
void Model::DrawSkeletonAll()
{
    if (!g_initialized || !g_hasViewProj) return;

    for (auto& m : g_models)
    {
        if (m.used && m.ufbx)
        {
            XMMATRIX world = m.transform.GetWorldMatrix();
            m.ufbx->DrawSkeleton(world, g_viewMatrix, g_projMatrix);
        }
    }
}

void Model::SetViewProj(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj)
{
    g_viewMatrix = view;
    g_projMatrix = proj;
    g_hasViewProj = true;
}

// ================================================================
// RayCast（未実装のダミー）
// ================================================================
void Model::RayCast(int handle, RayCastData* data)
{
    // 必要になったとき実装
}

