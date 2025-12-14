#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <memory>
#include <vector>     // ★変更：キュー用
#include <d3d11.h>
#include <DirectXColors.h>

#include "Gfx.h"
#include "Model.h"    // ★変更：Model::Draw/DrawSkeleton を呼ぶため（handle互換） :contentReference[oaicite:2]{index=2}

class Renderer
{
public:
    Renderer() = default;
    ~Renderer() = default;

    void Initialize();
    void OnResize(unsigned w, unsigned h);
    void BeginFrame();
    void EndFrame();
    void Present();

    //============================================================
    // ★変更：描画要求キュー（フェーズ1：3Dだけ実動。2D/UIは箱）
    //  - 既存の描画経路は一切変更しない（未使用でもビルド通る）
    //============================================================
    void SubmitModel3D(int modelHandle, bool drawSkeleton = false);
    void Submit2D();   // placeholder
    void SubmitUI();   // placeholder

    // キュー実行（App側が呼ぶのは後のステップ。今は未使用）
    void Execute();

private:
    //============================================================
    // ★変更：内部キュー
    //============================================================
    struct ModelDrawCmd
    {
        int  handle = -1;
        bool drawSkeleton = false;
    };

    std::vector<ModelDrawCmd> m_queue3D;
    bool m_has2D = false; // placeholder
    bool m_hasUI = false; // placeholder

private:
    //DirectX::CommonStates* States() const { return m_states.get(); }
    //std::shared_ptr<DirectX::CommonStates> m_states;
    float m_clear[4]{ 0.1f, 0.18f, 0.25f, 1.0f };
};
