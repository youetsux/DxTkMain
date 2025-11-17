// Model.cpp — anonymous namespace at file top (file-scope internal state)
#include "Model.h"

#include <vector>
#include <memory>
#include <cstring>
#include <cmath>
#include <d3d11.h>

#include "Gfx.h"
#include "ufbx.h"

using namespace DirectX;

namespace
{
    // file-scope internal state (clearly file-local)
    std::vector<Model::ModelData> g_models;
    bool g_initialized = false;

    // 仮のカメラ行列（ファイル内限定）
    XMMATRIX g_viewMatrix = XMMatrixIdentity();
    XMMATRIX g_projMatrix = XMMatrixIdentity();
    bool     g_hasViewProj = false;

    // helper: frame (60fps) -> time in seconds
    inline double FrameToTime(int frame) { return static_cast<double>(frame) / 60.0; }
}

namespace Model
{
    void Initialize(size_t maxCount)
    {
        g_models.clear();
        g_models.resize(maxCount);
        g_initialized = true;
    }

    void AllRelease()
    {
        for (auto& m : g_models) {
            m.ufbx.reset();
            m.scene.reset();
            m.used = false;
            m.fileName.clear();
            m.transform = Transform{};
            m.animInfo = AnimState{};
        }
        g_initialized = false;
    }

    void Delete(int handle)
    {
        if (handle < 0 || handle >= static_cast<int>(g_models.size())) return;
        auto& m = g_models[handle];
        m.ufbx.reset();
        m.scene.reset();
        m.used = false;
        m.fileName.clear();
        m.transform = Transform{};
        m.animInfo = AnimState{};
    }

    int LoadUfbx(const char* fbxPath)
    {
        if (!fbxPath) return -1;
        if (!g_initialized) Initialize(32);

        for (size_t i = 0; i < g_models.size(); ++i) {
            if (!g_models[i].used) {
                ModelData& m = g_models[i];

                m.used = true;
                m.fileName = fbxPath;

                //-------------------------------------------------------
                // 1) FBX 生シーンデータの読み込み
                //-------------------------------------------------------
                ufbx_error err;
                ufbx_load_opts opts;
                std::memset(&err, 0, sizeof(err));
                std::memset(&opts, 0, sizeof(opts));

                opts.target_axes = ufbx_axes_left_handed_y_up;
                opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;

                ufbx_scene* rawScene = ufbx_load_file(fbxPath, &opts, &err);
                if (!rawScene) {
                    m.used = false;
                    return -1;
                }

                m.scene = std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)>(
                    rawScene, ufbx_free_scene);

                //-------------------------------------------------------
                // 2) アニメ情報の初期化（60fps前提）
                //-------------------------------------------------------
                m.animationFps = 60.0f;
                m.animInfo = AnimState{}; // reset

                if (m.scene && m.scene->anim) {
                    const ufbx_anim* anim = m.scene->anim;
                    double begin = anim->time_begin;
                    double end = anim->time_end;
                    double duration = end - begin;
                    if (duration < 0.0) duration = 0.0;

                    m.animInfo.beginTime = begin;
                    m.animInfo.endTime = end;
                    m.animInfo.totalFrames = static_cast<int>(std::ceil(duration * 60.0));

                    m.animInfo.startFrame = 0;
                    m.animInfo.endFrame = (m.animInfo.totalFrames > 0) ? (m.animInfo.totalFrames - 1) : 0;
                    m.animInfo.currentFrame = 0.0f;
                    m.animInfo.speed = 1.0f;
                    m.animInfo.loop = true;
                }
                else {
                    // no animation
                    m.animInfo.beginTime = 0.0;
                    m.animInfo.endTime = 0.0;
                    m.animInfo.totalFrames = 0;
                    m.animInfo.startFrame = 0;
                    m.animInfo.endFrame = 0;
                    m.animInfo.currentFrame = 0.0f;
                    m.animInfo.speed = 1.0f;
                    m.animInfo.loop = true;
                }

                //-------------------------------------------------------
                // 3) メッシュ部分の読み込み（既存の UfbxStaticModel）
                //-------------------------------------------------------
                m.ufbx = std::make_unique<UfbxStaticModel>();
                if (!m.ufbx->Load(fbxPath)) {
                    m.used = false;
                    m.ufbx.reset();
                    m.scene.reset();
                    return -1;
                }

                return static_cast<int>(i);
            }
        }

        return -1;
    }

    void SetTransform(int handle, const Transform& transform)
    {
        if (handle < 0 || handle >= static_cast<int>(g_models.size())) return;
        auto& m = g_models[handle];
        if (!m.used) return;
        m.transform = transform;
    }

    XMMATRIX GetMatrix(int handle)
    {
        if (handle < 0 || handle >= static_cast<int>(g_models.size())) return XMMatrixIdentity();
        auto& m = g_models[handle];
        if (!m.used) return XMMatrixIdentity();
        m.transform.Calclation();
        return m.transform.GetWorldMatrix();
    }

    // カメラ行列を保存（ファイルスコープの g_viewMatrix/g_projMatrix を更新）
    void SetViewProjection(const XMMATRIX& view, const XMMATRIX& proj)
    {
        g_viewMatrix = view;
        g_projMatrix = proj;
        g_hasViewProj = true;
    }

    // 描画（内部 Transform を使う）
    void DrawUfbx(int handle)
    {
        if (!g_hasViewProj) return;
        if (handle < 0 || handle >= static_cast<int>(g_models.size())) return;

        ModelData& m = g_models[handle];
        if (!m.used || !m.ufbx) return;

        m.transform.Calclation();
        XMMATRIX world = m.transform.GetWorldMatrix();

        // 現状 DrawUfbx は「既に評価済のスケルトン状態を描く」動作
        m.ufbx->Draw(world, g_viewMatrix, g_projMatrix);
    }

    // Transform 指定で描画（内部 Transform は変更しない）
    void DrawUfbx(int handle, const Transform& transform)
    {
        if (!g_hasViewProj) return;
        if (handle < 0 || handle >= static_cast<int>(g_models.size())) return;

        ModelData& m = g_models[handle];
        if (!m.used || !m.ufbx) return;

        Transform tmp = transform;
        tmp.Calclation();
        XMMATRIX world = tmp.GetWorldMatrix();

        m.ufbx->Draw(world, g_viewMatrix, g_projMatrix);
    }

    // フレーム指定でアニメを評価して描画（内部 Transform を使う）
    void DrawUfbxAtFrame(int handle, int frame)
    {
        if (!g_hasViewProj) return;
        if (handle < 0 || handle >= static_cast<int>(g_models.size())) return;

        ModelData& m = g_models[handle];
        if (!m.used || !m.ufbx) return;

        if (m.scene && m.scene->anim && m.animInfo.totalFrames > 0) {
            int f = frame;
            if (f < m.animInfo.startFrame) f = m.animInfo.startFrame;
            if (f > m.animInfo.endFrame)   f = m.animInfo.endFrame;

            double t = m.animInfo.beginTime + FrameToTime(f);
            if (t < m.animInfo.beginTime) t = m.animInfo.beginTime;
            if (t > m.animInfo.endTime)   t = m.animInfo.endTime;

            m.ufbx->UpdateSkeletonAtTime(m.scene.get(), t);
        }

        m.transform.Calclation();
        XMMATRIX world = m.transform.GetWorldMatrix();
        m.ufbx->Draw(world, g_viewMatrix, g_projMatrix);
    }

    // フレーム指定で描画（指定 Transform を使う）
    void DrawUfbxAtFrame(int handle, const Transform& transform, int frame)
    {
        if (!g_hasViewProj) return;
        if (handle < 0 || handle >= static_cast<int>(g_models.size())) return;

        ModelData& m = g_models[handle];
        if (!m.used || !m.ufbx) return;

        if (m.scene && m.scene->anim && m.animInfo.totalFrames > 0) {
            int f = frame;
            if (f < m.animInfo.startFrame) f = m.animInfo.startFrame;
            if (f > m.animInfo.endFrame)   f = m.animInfo.endFrame;

            double t = m.animInfo.beginTime + FrameToTime(f);
            if (t < m.animInfo.beginTime) t = m.animInfo.beginTime;
            if (t > m.animInfo.endTime)   t = m.animInfo.endTime;

            m.ufbx->UpdateSkeletonAtTime(m.scene.get(), t);
        }

        Transform tmp = transform;
        tmp.Calclation();
        XMMATRIX world = tmp.GetWorldMatrix();
        m.ufbx->Draw(world, g_viewMatrix, g_projMatrix);
    }

    void DrawUfbxAll()
    {
        if (!g_hasViewProj) return;

        for (size_t i = 0; i < g_models.size(); ++i) {
            if (!g_models[i].used || !g_models[i].ufbx) continue;
            g_models[i].transform.Calclation();
            XMMATRIX world = g_models[i].transform.GetWorldMatrix();

            g_models[i].ufbx->Draw(world, g_viewMatrix, g_projMatrix);
        }
    }

    void DrawSkeletonAll()
    {
        if (!g_hasViewProj) return;

        for (size_t i = 0; i < g_models.size(); ++i) {
            if (!g_models[i].used || !g_models[i].ufbx) continue;
            g_models[i].transform.Calclation();
            XMMATRIX world = g_models[i].transform.GetWorldMatrix();

            g_models[i].ufbx->DrawSkeleton(world, g_viewMatrix, g_projMatrix);
        }
    }

    void RayCast(int handle, RayCastData* data)
    {
        (void)handle;
        (void)data;
    }
} // namespace Model
