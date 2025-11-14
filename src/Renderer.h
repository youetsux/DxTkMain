#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <memory>
#include <d3d11.h>
#include <DirectXColors.h>
#include <CommonStates.h>
#include <Effects.h>
#include "Gfx.h"

namespace DirectX { namespace DX11 { class BasicEffect; } }

class Renderer
{
public:
    Renderer() = default;
    ~Renderer() = default;

    // 初期化（Gfx::Init() 済み前提）
    void Initialize();

    // ウィンドウリサイズ時の処理
    void OnResize(unsigned w, unsigned h);

    // 1フレームの描画開始／終了／Present
    void BeginFrame();
    void EndFrame();
    void Present();

    // 共有ステートオブジェクト（不透明 / デプス / ラスタライズなど）
    DirectX::CommonStates* States() const { return m_states.get(); }

    // 共有 BasicEffect（モデル側から使う）
    DirectX::DX11::BasicEffect* BasicFx() const { return m_basicFx.get(); }

private:
    // 共通ステート（ブレンド / デプス / ラスタライズ）
    std::unique_ptr<DirectX::CommonStates>       m_states;

    // 共有 BasicEffect（全モデルで共用）
    std::unique_ptr<DirectX::DX11::BasicEffect>  m_basicFx;

    // クリアカラー
    float m_clear[4]{ 0.1f, 0.18f, 0.25f, 1.0f };
};
