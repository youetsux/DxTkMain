#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <DirectXMath.h>

// ページのインターフェース準拠
// Initialize / Update / SetPosition / SetTarget / GetViewMatrix / GetProjectionMatrix
// 追加: SetPerspective / OnResize は「将来拡張」用（未使用なら呼ばなくてOK）
namespace Camera
{
    using namespace DirectX;

    // 初期化（プロジェクション行列を作成）
    void Initialize();

    // 更新（位置・向きが変わったときにビュー行列を再構成）
    void Update();

    // 視点（カメラの位置）を設定
    void SetPosition(XMVECTOR position);

    // 焦点（見る位置）を設定
    void SetTarget(XMVECTOR target);

    // ビュー行列を取得
    XMMATRIX GetViewMatrix();

    // プロジェクション行列を取得
    XMMATRIX GetProjectionMatrix();

    // --- ここからは拡張（任意）---
    // 透視投影の設定（既定: FOV=45°, 近0.1, 遠1000）
    void SetPerspective(float fovYRadians, float aspect, float zn = 0.1f, float zf = 1000.0f);

    // ウィンドウサイズ変更時にアスペクトを更新
    void OnResize(unsigned width, unsigned height);
}
