#include "Model.h"

using namespace DirectX;

namespace
{
    constexpr float  DEFAULT_ANIM_FPS = 60.0f;
    constexpr size_t DEFAULT_MODEL_SLOTS = 32;

    // モデル配列（ハンドル = インデックス）
    std::vector<Model::ModelData> g_models;
    bool g_initialized = false;

    // 共通 view / proj
    XMMATRIX g_viewMatrix = XMMatrixIdentity();
    XMMATRIX g_projMatrix = XMMatrixIdentity();
    bool     g_hasViewProj = false;

    // ---------------------------------------
    // 内部ヘルパ
    // ---------------------------------------

    inline bool IsValidHandle(int handle)
    {
        return handle >= 0 && handle < static_cast<int>(g_models.size());
    }

    // frame → timeSec 変換
    double FrameToTimeSec(const Model::ModelData& m, int frame)
    {
        const Model::AnimState& a = m.animInfo;

        if (a.totalFrames <= 0)
        {
            return a.beginTime;    // アニメなし
        }

        const double fps = static_cast<double>(m.animationFps);
        const double t0 = a.beginTime;
        double       t_sec = t0 + static_cast<double>(frame) / fps;

        if (a.endTime > a.beginTime)
        {
            if (t_sec < a.beginTime) t_sec = a.beginTime;
            if (t_sec > a.endTime)   t_sec = a.endTime;
        }

        return t_sec;
    }

    // currentFrame を進めて整数フレームを返す
    int AdvanceAnimFrame(Model::ModelData& m)
    {
        Model::AnimState& a = m.animInfo;

        if (a.totalFrames <= 0)
        {
            return 0;
        }

        a.currentFrame += a.speed;

        if (a.loop)
        {
            if (a.currentFrame > static_cast<float>(a.endFrame))
            {
                a.currentFrame = static_cast<float>(a.startFrame);
            }
        }
        else
        {
            if (a.currentFrame > static_cast<float>(a.endFrame))
            {
                a.currentFrame = static_cast<float>(a.endFrame);
            }
        }

        int frame = static_cast<int>(a.currentFrame);
        if (frame < a.startFrame) frame = a.startFrame;
        if (frame > a.endFrame)   frame = a.endFrame;

        return frame;
    }

    // メイン描画ヘルパ（Transform と timeSec を受け取る）
    void DrawInternal(Model::ModelData& m,
        const Transform& transform,
        double             timeSec)
    {
        if (!m.used)                 return;
        if (!m.ufbx)                 return;
        if (!g_hasViewProj)          return;

        m.ufbx->UpdateSkeletonAtTime(timeSec);

        XMMATRIX world = transform.GetWorldMatrix();
        m.ufbx->Draw(world, g_viewMatrix, g_projMatrix);
    }

} // anonymous namespace

// ------------------------------------------------------------
// モジュール初期化
// ------------------------------------------------------------
void Model::Initialize(std::size_t maxCount)
{
    if (maxCount == 0)
        maxCount = DEFAULT_MODEL_SLOTS;

    g_models.clear();
    g_models.resize(maxCount);   // used=false で埋まる

    g_initialized = true;
    g_hasViewProj = false;
}

// ------------------------------------------------------------
// 全破棄
// ------------------------------------------------------------
void Model::AllRelease()
{
    if (!g_initialized) return;

    for (auto& m : g_models)
    {
        m.used = false;
        m.ufbx.reset();
        m.fileName.clear();
        m.animInfo = AnimState{};
    }

    g_models.clear();
    g_initialized = false;
    g_hasViewProj = false;
}

// ------------------------------------------------------------
// 単体削除
// ------------------------------------------------------------
void Model::Delete(int handle)
{
    if (!g_initialized) return;
    if (!IsValidHandle(handle)) return;

    ModelData& m = g_models[handle];
    if (!m.used) return;

    m.used = false;
    m.ufbx.reset();
    m.fileName.clear();
    m.animInfo = AnimState{};
}

// ------------------------------------------------------------
// Transform 設定
// ------------------------------------------------------------
void Model::SetTransform(int handle, const Transform& transform)
{
    if (!g_initialized) return;
    if (!IsValidHandle(handle)) return;

    ModelData& m = g_models[handle];
    if (!m.used) return;

    m.transform = transform;
}

// ------------------------------------------------------------
// Transform → 行列
// ------------------------------------------------------------
XMMATRIX Model::GetMatrix(int handle)
{
    if (!g_initialized)          return XMMatrixIdentity();
    if (!IsValidHandle(handle))  return XMMatrixIdentity();

    const ModelData& m = g_models[handle];
    if (!m.used)                 return XMMatrixIdentity();

    return m.transform.GetWorldMatrix();
}

// ------------------------------------------------------------
// UFBX モデル読み込み（実体は FbxModel）
// ------------------------------------------------------------
int Model::LoadUfbx(const char* fbxPath)
{
    if (!fbxPath) return -1;

    if (!g_initialized)
        Initialize(DEFAULT_MODEL_SLOTS);

    // 空きスロットを探す
    int freeIndex = -1;
    for (int i = 0; i < static_cast<int>(g_models.size()); ++i)
    {
        if (!g_models[i].used)
        {
            freeIndex = i;
            break;
        }
    }
    if (freeIndex < 0)
    {
        // 空きがない場合は拡張
        freeIndex = static_cast<int>(g_models.size());
        g_models.emplace_back();
    }

    ModelData& m = g_models[freeIndex];
    m.used = true;
    m.fileName = fbxPath;
    m.transform = Transform();
    m.ufbx = std::make_unique<FbxModel>();

    // FbxModel::Load
    if (!m.ufbx->Load(fbxPath))
    {
        m.used = false;
        m.ufbx.reset();
        m.fileName.clear();
        return -1;
    }

    // アニメーション情報の初期化
    m.animationFps = DEFAULT_ANIM_FPS;
    m.animInfo = AnimState{};

    if (const ufbx_anim* anim = m.ufbx->GetDefaultAnim())
    {
        AnimState& a = m.animInfo;

        a.beginTime = anim->time_begin;
        a.endTime = anim->time_end;

        const double span = anim->time_end - anim->time_begin;
        if (span > 0.0)
        {
            a.totalFrames = static_cast<int>(span * m.animationFps + 0.5);
            if (a.totalFrames < 1) a.totalFrames = 1;

            a.startFrame = 0;
            a.endFrame = a.totalFrames - 1;
            a.currentFrame = 0.0f;
            a.speed = 1.0f;
            a.loop = true;
        }
    }

    return freeIndex;
}

// ------------------------------------------------------------
// view / proj 設定
// ------------------------------------------------------------
void Model::SetViewProj(const XMMATRIX& view, const XMMATRIX& proj)
{
    g_viewMatrix = view;
    g_projMatrix = proj;
    g_hasViewProj = true;
}

// ------------------------------------------------------------
// UFBX 単体描画（内部 view/proj を利用・Transform 内部使用）
// ------------------------------------------------------------
void Model::DrawUfbx(int handle)
{
    if (!g_initialized || !g_hasViewProj) return;
    if (!IsValidHandle(handle))          return;

    ModelData& m = g_models[handle];
    if (!m.used || !m.ufbx) return;

    int frame = AdvanceAnimFrame(m);
    double t_sec = FrameToTimeSec(m, frame);

    DrawInternal(m, m.transform, t_sec);
}

// ------------------------------------------------------------
// Transform を都度指定して描画
// ------------------------------------------------------------
void Model::DrawUfbx(int handle, const Transform& transform)
{
    if (!g_initialized || !g_hasViewProj) return;
    if (!IsValidHandle(handle))          return;

    ModelData& m = g_models[handle];
    if (!m.used || !m.ufbx) return;

    int frame = AdvanceAnimFrame(m);
    double t_sec = FrameToTimeSec(m, frame);

    DrawInternal(m, transform, t_sec);
}

// ------------------------------------------------------------
// フレーム指定（基本）
// ------------------------------------------------------------
void Model::DrawUfbx(int handle, const Transform& transform, int frame)
{
    if (!g_initialized || !g_hasViewProj) return;
    if (!IsValidHandle(handle))          return;

    ModelData& m = g_models[handle];
    if (!m.used || !m.ufbx) return;

    double t_sec = FrameToTimeSec(m, frame);
    DrawInternal(m, transform, t_sec);
}

// ------------------------------------------------------------
// 現在の transform を使うバージョン
// ------------------------------------------------------------
void Model::DrawUfbx(int handle, int frame)
{
    if (!g_initialized || !g_hasViewProj) return;
    if (!IsValidHandle(handle))          return;

    ModelData& m = g_models[handle];
    if (!m.used || !m.ufbx) return;

    double t_sec = FrameToTimeSec(m, frame);
    DrawInternal(m, m.transform, t_sec);
}

// ------------------------------------------------------------
// UFBX 全体描画（view/proj を内部に保存）
// ------------------------------------------------------------
void Model::DrawUfbxAll(const XMMATRIX& view, const XMMATRIX& proj)
{
    SetViewProj(view, proj);

    if (!g_initialized) return;

    for (int i = 0; i < static_cast<int>(g_models.size()); ++i)
    {
        if (!g_models[i].used) continue;
        DrawUfbx(i);
    }
}

// ------------------------------------------------------------
// スケルトン描画（同じく内部 view/proj 利用）
// ------------------------------------------------------------
void Model::DrawSkeletonAll()
{
    if (!g_initialized || !g_hasViewProj) return;

    for (auto& m : g_models)
    {
        if (!m.used || !m.ufbx) continue;

        XMMATRIX world = m.transform.GetWorldMatrix();
        m.ufbx->DrawSkeleton(world, g_viewMatrix, g_projMatrix);
    }
}

// ------------------------------------------------------------
// レイキャスト（FbxModel 側が対応したらここから呼ぶ）
// ------------------------------------------------------------
void Model::RayCast(int handle, RayCastData* data)
{
    // まだ FbxModel に RayCast がなければダミー
    (void)handle;
    (void)data;
}
