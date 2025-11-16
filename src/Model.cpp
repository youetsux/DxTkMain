#pragma once

#include <DirectXMath.h>
#include <string>
#include <vector>
#include <memory>
#include "ufbx.h"

#include "Transform.h"
#include "UfbxStaticModel.h"

struct RayCastData;

namespace Model
{
    //struct ModelData
    //{
    //    bool used = false;
    //    std::string fileName;
    //    Transform transform;
    //    std::unique_ptr<UfbxStaticModel> ufbx;
    //};

    void Initialize(size_t maxCount = 32);
    void AllRelease();
    void Delete(int handle);
    int LoadUfbx(const char* fbxPath);
    void SetTransform(int handle, const Transform& transform);
    DirectX::XMMATRIX GetMatrix(int handle);

    // 新規追加：単体モデル描画（Transform を使用する）
    void DrawUfbx(int handle, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj);

    // 従来どおり：全モデル描画
    void DrawUfbxAll(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj);
    void DrawSkeletonAll(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj);
    void RayCast(int handle, RayCastData* data);
}

//--------------------------------------
// Model.cpp
//--------------------------------------
#include "Model.h"
#include "Gfx.h"

namespace Model
{
    static std::vector<ModelData> g_models;

    void Initialize(size_t maxCount)
    {
        g_models.clear();
        g_models.resize(maxCount);
    }

    void AllRelease()
    {
        for (auto& m : g_models) {
            m.ufbx.reset();
            m.used = false;
        }
    }

    void Delete(int handle)
    {
        if (handle < 0 || handle >= (int)g_models.size()) return;
        g_models[handle].ufbx.reset();
        g_models[handle].used = false;
    }

    int LoadUfbx(const char* fbxPath)
    {
        for (size_t i = 0; i < g_models.size(); ++i) {
            if (!g_models[i].used) {

                g_models[i].used = true;
                g_models[i].fileName = fbxPath;

                //-------------------------------------------------------
                // ★ 1) FBX 生シーンデータの読み込み
                //-------------------------------------------------------
                ufbx_error err;
                ufbx_load_opts opts;
                memset(&err, 0, sizeof(err));
                memset(&opts, 0, sizeof(opts));

                // 今のエンジンに合わせた座標変換（UfbxStaticModel と同じ）
                opts.target_axes = ufbx_axes_left_handed_y_up;
                opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;

                ufbx_scene* rawScene = ufbx_load_file(fbxPath, &opts, &err);
                if (!rawScene) {
                    g_models[i].used = false;
                    return -1;
                }

                // ★ 生シーンを unique_ptr に配置
                g_models[i].scene =
                    std::unique_ptr<ufbx_scene, void(*)(ufbx_scene*)>(
                        rawScene, ufbx_free_scene);

                //-------------------------------------------------------
                // ★ 2) FPS を Scene から抽出
                //-------------------------------------------------------
                if (g_models[i].scene && g_models[i].scene->anim && g_models[i].scene->anim->fps > 0.0) {
                    g_models[i].animationFps = (float)g_models[i].scene->anim->fps;
                }
                else {
                    g_models[i].animationFps = 30.0f; // fallback
                }

                //-------------------------------------------------------
                // ★ 3) メッシュ部分の読み込み（既存の UfbxStaticModel）
                //-------------------------------------------------------
                g_models[i].ufbx = std::make_unique<UfbxStaticModel>();
                if (!g_models[i].ufbx->Load(fbxPath)) {
                    g_models[i].used = false;
                    g_models[i].ufbx.reset();
                    g_models[i].scene.reset();
                    return -1;
                }

                return (int)i;
            }
        }
        return -1;
    }

    void SetTransform(int handle, const Transform& transform)
    {
        if (handle < 0 || handle >= (int)g_models.size()) return;
        g_models[handle].transform = transform;
    }

    DirectX::XMMATRIX GetMatrix(int handle)
    {
        using namespace DirectX;
        if (handle < 0 || handle >= (int)g_models.size()) return XMMatrixIdentity();
        auto& e = g_models[handle];
        e.transform.Calclation();
        return e.transform.GetWorldMatrix();
    }

    //============================================
    // 新規実装：Transform を使った単体描画
    //============================================
    void DrawUfbx(int handle, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj)
    {
        using namespace DirectX;
        if (handle < 0 || handle >= (int)g_models.size()) return;
        auto& e = g_models[handle];
        if (!e.used || !e.ufbx) return;

        e.transform.Calclation();
        XMMATRIX world = e.transform.GetWorldMatrix();

        e.ufbx->Draw(world, view, proj);
    }

    //============================================
    // 全体描画（既存）
    //============================================
    void DrawUfbxAll(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj)
    {
        using namespace DirectX;
        for (auto& e : g_models) {
            if (!e.used || !e.ufbx) continue;
            e.transform.Calclation();
            XMMATRIX world = e.transform.GetWorldMatrix();
            e.ufbx->Draw(world, view, proj);
        }
    }

    void DrawSkeletonAll(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj)
    {
        using namespace DirectX;
        for (auto& e : g_models) {
            if (!e.used || !e.ufbx) continue;
            e.transform.Calclation();
            XMMATRIX world = e.transform.GetWorldMatrix();
            e.ufbx->DrawSkeleton(world, view, proj);
        }
    }

    void RayCast(int handle, RayCastData* data)
    {
        // 未実装
    }
}
