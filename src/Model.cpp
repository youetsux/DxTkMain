#include "Model.h"

#include "Gfx.h"        // Gfx::Dev(), Gfx::Ctx()
#include "Renderer.h"   // Renderer::States(), Renderer::BasicFx()
#include <Effects.h>

namespace Model
{
    namespace
    {
        std::vector<ModelData> g_models;

        bool IsValid(int h)
        {
            return h >= 0 &&
                h < (int)g_models.size() &&
                g_models[h].used &&
                g_models[h].ufbx;
        }
    }

    //======================================================================
    // 初期化
    //======================================================================
    void Initialize(size_t maxCount)
    {
        g_models.clear();
        g_models.resize(maxCount);
    }

    //======================================================================
    // 全削除
    //======================================================================
    void AllRelease()
    {
        for (auto& m : g_models) {
            m.ufbx.reset();
            m.used = false;
            m.fileName.clear();
        }
    }

    //======================================================================
    // 1つ削除
    //======================================================================
    void Delete(int h)
    {
        if (!IsValid(h)) return;
        g_models[h] = ModelData{};
    }

    //======================================================================
    // LoadUfbx（BasicEffect を持たない・Renderer の共有 fx を使う）
    //======================================================================
    int LoadUfbx(const char* fbxPath)
    {
        if (!fbxPath || !*fbxPath) return -1;

        auto* device = Gfx::Dev();
        auto* fx = Renderer::BasicFx();    // ★ BasicEffect は共有物を使う

        if (!device || !fx) return -1;

        // 空きスロットを探す
        int slot = -1;
        for (int i = 0; i < (int)g_models.size(); ++i) {
            if (!g_models[i].used) { slot = i; break; }
        }
        if (slot < 0) {
            slot = (int)g_models.size();
            g_models.emplace_back();
        }

        // UFBX モデル生成
        auto mdl = std::make_unique<UfbxStaticModel>();
        if (!mdl->Load(device, fbxPath, fx)) {
            return -1;
        }

        // スロットへ登録
        auto& e = g_models[slot];
        e.used = true;
        e.fileName = fbxPath;
        e.transform = Transform();
        e.ufbx = std::move(mdl);

        return slot;
    }

    //======================================================================
    // トランスフォーム
    //======================================================================
    void SetTransform(int h, Transform& t)
    {
        if (!IsValid(h)) return;
        g_models[h].transform = t;
    }

    DirectX::XMMATRIX GetMatrix(int h)
    {
        if (!IsValid(h)) return DirectX::XMMatrixIdentity();
        return g_models[h].transform.GetMatrix();
    }

    //======================================================================
    // 描画（ctx / states / fx を内部で取得）
    //======================================================================
    void DrawUfbxAll(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj)
    {
        auto* ctx = Gfx::Ctx();
        auto* states = Renderer::States();
        auto* fx = Renderer::BasicFx();

        if (!ctx || !states || !fx) return;

        for (auto& m : g_models) {
            if (!m.used || !m.ufbx) continue;

            auto world = m.transform.GetMatrix();
            m.ufbx->Draw(ctx, world, view, proj, states, fx);
        }
    }

    //======================================================================
    // スケルトン描画（デバッグ）
    //======================================================================
    void DrawSkeletonAll(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj)
    {
        auto* ctx = Gfx::Ctx();
        if (!ctx) return;

        for (auto& m : g_models) {
            if (!m.used || !m.ufbx) continue;

            auto world = m.transform.GetMatrix();
            m.ufbx->DrawSkeleton(ctx, world, view, proj);
        }
    }

    //======================================================================
    // レイキャスト（必要な時だけ）
    //======================================================================
    void RayCast(int handle, RayCastData* data)
    {
        if (!IsValid(handle) || !data) return;

        // 必要なら UfbxStaticModel 側に関数を追加して実装
        // 今は空実装のまま
    }
}
