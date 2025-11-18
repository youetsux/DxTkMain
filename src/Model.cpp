#include "Model.h"
#include "Gfx.h"

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
        m.scene.reset();
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
    m.scene.reset();
}

// ================================================================
// Transform 設定
// ================================================================
void Model::SetTransform(int handle,const Transform& transform)
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
        // 1) ufbx 生シーン読み込み
        // ------------------------------
        ufbx_error err{};
        ufbx_load_opts opts{};
        opts.target_axes = ufbx_axes_left_handed_y_up;
        opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;

        ufbx_scene* rawScene = ufbx_load_file(fbxPath, &opts, &err);
        if (!rawScene)
        {
            m.used = false;
            return -1;
        }

        m.scene = std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)>(rawScene, ufbx_free_scene);

        // ------------------------------
        // 2) アニメ FPS の決定
        // ------------------------------
        m.animationFps = MODEL_ANIMATION_FPS;

        if (m.scene && m.scene->anim)
        {
            double begin = m.scene->anim->time_begin;
            double end = m.scene->anim->time_end;

            double sec = (end - begin);
            int totalFrames = (int)std::round(sec * MODEL_ANIMATION_FPS);

            m.animInfo.beginTime = begin;
            m.animInfo.endTime = end;
            m.animInfo.totalFrames = totalFrames;
            m.animInfo.startFrame = 0;
            m.animInfo.endFrame = totalFrames - 1;
        }

        // ------------------------------
        // 3) メッシュ読み込み (UfbxStaticModel)
        // ------------------------------
        m.ufbx = std::make_unique<UfbxStaticModel>();
        if (!m.ufbx->Load(fbxPath))
        {
            m.used = false;
            m.scene.reset();
            m.ufbx.reset();
            return -1;
        }

        return (int)i;
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

    XMMATRIX world = m.transform.GetWorldMatrix();

    m.ufbx->Draw(world, g_viewMatrix, g_projMatrix);
}

// ================================================================
// Transform を指定して描画
// ================================================================
void Model::DrawUfbx(int handle,const Transform& transform)
{
    if (!g_initialized || !g_hasViewProj) return;
    if (handle < 0 || handle >= (int)g_models.size()) return;

    ModelData& m = g_models[handle];
    if (!m.used || !m.ufbx) return;

    XMMATRIX world = transform.GetWorldMatrix();
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
            DrawUfbx((int)i);
    }
}

//-------------------------------------------------------------
// 単体描画（Transform 指定・フレーム指定）
//-------------------------------------------------------------
void DrawUfbx(int handle, const Transform& transform, int frame)
{
    if (handle < 0) return;
    if (static_cast<size_t>(handle) >= g_models.size()) return;

    Model::ModelData& m = g_models[handle];
    if (!m.used || !m.ufbx) return;

    // Transform → ワールド行列
    DirectX::XMMATRIX world = transform.GetWorldMatrix();

    // アニメーションが存在する場合のみ更新
    if (m.scene && m.scene->anim)
    {
        const ufbx_anim* anim = m.scene->anim;

        // FPS（固定 60）
        const double fps = 60.0;

        // frame → 秒変換
        double t_sec = anim->time_begin + (frame / fps);

        // アニメ時間を Clamp
        if (t_sec < anim->time_begin) t_sec = anim->time_begin;
        if (t_sec > anim->time_end)   t_sec = anim->time_end;

        // スケルトン更新
        m.ufbx->UpdateSkeletonAtTime(m.scene.get(), anim, t_sec);
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

// ================================================================
// RayCast（未実装のダミー）
// ================================================================
void Model::RayCast(int handle, RayCastData* data)
{
    // 必要になったとき実装
}
