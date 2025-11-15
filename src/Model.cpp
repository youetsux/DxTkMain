#include "Model.h"

#include <d3d11.h>

#include "Gfx.h"        // Gfx::Dev(), Gfx::Ctx() がある前提

using namespace DirectX;

namespace Model
{
    namespace
    {
        std::vector<ModelData> g_models;
        bool g_initialized = false;

        // メッシュ表示 ON/OFF（必要なら外部から制御する）
        bool g_showMesh = true;
    }

    void Initialize(size_t maxCount)
    {
        g_models.clear();
        g_models.resize(maxCount);
        g_initialized = true;
    }

    void AllRelease()
    {
        for (auto& m : g_models)
        {
            m.ufbx.reset();
            m.used = false;
            m.fileName.clear();
            m.transform = Transform{};
        }
        g_initialized = false;
    }

    void Delete(int handle)
    {
        if (handle < 0 || handle >= static_cast<int>(g_models.size()))
            return;

        auto& m = g_models[handle];
        m.ufbx.reset();
        m.used = false;
        m.fileName.clear();
        m.transform = Transform{};
    }

    int LoadUfbx(const char* fbxPath)
    {
        if (!fbxPath)
            return -1;

        if (!g_initialized)
            Initialize();

        ID3D11Device* device = Gfx::Dev();
        if (!device)
            return -1;

        // 空きスロット検索
        int handle = -1;
        for (size_t i = 0; i < g_models.size(); ++i)
        {
            if (!g_models[i].used)
            {
                handle = static_cast<int>(i);
                break;
            }
        }
        if (handle < 0)
            return -1;

        auto& slot = g_models[handle];
        slot.ufbx = std::make_unique<UfbxStaticModel>();

        // ★ BasicEffect は使わないので第3引数は nullptr 固定
        if (!slot.ufbx->Load(fbxPath))
        {
            slot.ufbx.reset();
            return -1;
        }

        slot.used = true;
        slot.fileName = fbxPath;
        slot.transform = Transform{};   // デフォルト姿勢

        return handle;
    }

    void SetTransform(int handle, const Transform& transform)
    {
        if (handle < 0 || handle >= static_cast<int>(g_models.size()))
            return;

        auto& m = g_models[handle];
        if (!m.used)
            return;

        m.transform = transform;
    }

    XMMATRIX GetMatrix(int handle)
    {
        if (handle < 0 || handle >= static_cast<int>(g_models.size()))
            return XMMatrixIdentity();

        auto& m = g_models[handle];
        if (!m.used)
            return XMMatrixIdentity();

        // Transform クラスの既存 API をそのまま利用
        // （毎フレームここで Calclation してしまう簡易版）
        m.transform.Calclation();
        return m.transform.GetWorldMatrix();
    }

    void DrawUfbxAll(const XMMATRIX& view, const XMMATRIX& proj)
    {
        if (!g_showMesh)
            return;

        ID3D11DeviceContext* ctx = Gfx::Ctx();
        if (!ctx)
            return;

        for (auto& e : g_models)
        {
            if (!e.used || !e.ufbx)
                continue;

            // 現在の Transform からワールド行列を取得
            e.transform.Calclation();
            XMMATRIX world = e.transform.GetWorldMatrix();

            // states / fx は nullptr → UfbxStaticModel 側でフォールバックさせる
            e.ufbx->Draw(world, view, proj);
        }
    }

    void DrawSkeletonAll(const XMMATRIX& view, const XMMATRIX& proj)
    {
        ID3D11DeviceContext* ctx = Gfx::Ctx();
        if (!ctx)
            return;

        for (auto& e : g_models)
        {
            if (!e.used || !e.ufbx)
                continue;

            e.transform.Calclation();
            XMMATRIX world = e.transform.GetWorldMatrix();

            e.ufbx->DrawSkeleton(world, view, proj);
        }
    }

    void RayCast(int handle, RayCastData* data)
    {
        // まだ UFBX 版の衝突判定は実装していないのでダミー
        (void)handle;
        (void)data;
    }
}
