#include "Dice.h"
#include "Gfx.h"
#include <SimpleMath.h>
#include "Camera.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

// Dice.png: 上段=1,2,3 / 下段=4,5,6
// 面の割当: 0:+X=6 (2,1) / 1:-X=1 (0,0) / 2:+Y=5 (1,1)
//           3:-Y=2 (1,0) / 4:+Z=4 (0,1) / 5:-Z=3 (2,0)
HRESULT Dice::Initialize()
{
    // 6面のUVを設定してInitialize
    // 3x4アトラス用
    struct F { int col, row; } uvmap[6] = {
        {2,3}, // +X → 4
        {0,3}, // -X → 3
        {1,0}, // +Y → 1
        {1,3}, // -Y → 5
        {1,2}, // +Z → 6
        {1,1}, // -Z → 2
    };

    for (int f = 0; f < 6; ++f)
    {

        float u_margin = 0.125f;               // 左端余白
        float du = (1.0f - u_margin * 2.0f) / 3.0f;  // 実際の1セル幅（=0.25）
        float dv = 1.0f / 4.0f;                // 縦4分割（等間隔）

        float u0 = u_margin + uvmap[f].col * du;
        float u1 = u0 + du;
        float v0 = uvmap[f].row * dv;          // ← 反転しない
        float v1 = v0 + dv;

        HRESULT hr = m_faces[f].Initialize(u0, v0, u1, v1);
        if (FAILED(hr)) return hr;
        hr = m_faces[f].LoadTexture(".\\Assets\\Dice.png");
        if (FAILED(hr)) return hr;
    }
    return S_OK;
}

void Dice::Draw(const XMMATRIX& wvpBase)
{
    // 回転（Quad は +Z 向き前提）
    const XMMATRIX R0 = XMMatrixRotationY(-XM_PIDIV2); // +X
    const XMMATRIX R1 = XMMatrixRotationY(+XM_PIDIV2); // -X
    const XMMATRIX R2 = XMMatrixRotationX(+XM_PIDIV2); // +Y
    const XMMATRIX R3 = XMMatrixRotationX(-XM_PIDIV2); // -Y
    const XMMATRIX R4 = XMMatrixIdentity();            // +Z
    const XMMATRIX R5 = XMMatrixRotationY(XM_PI);      // -Z

    // 中心から±0.5押し出し
    const XMMATRIX T0 = XMMatrixTranslation(+0.5f, 0.f, 0.f);
    const XMMATRIX T1 = XMMatrixTranslation(-0.5f, 0.f, 0.f);
    const XMMATRIX T2 = XMMatrixTranslation(0.f, +0.5f, 0.f);
    const XMMATRIX T3 = XMMatrixTranslation(0.f, -0.5f, 0.f);
    const XMMATRIX T4 = XMMatrixTranslation(0.f, 0.f, +0.5f);
    const XMMATRIX T5 = XMMatrixTranslation(0.f, 0.f, -0.5f);

    // ★ 回転→押し出し（R*T）、その後に全体の WVP を掛ける
    const XMMATRIX M[6] = {
        (R0 * T0) * wvpBase, // +X
        (R1 * T1) * wvpBase, // -X
        (R2 * T2) * wvpBase, // +Y
        (R3 * T3) * wvpBase, // -Y
        (R4 * T4) * wvpBase, // +Z
        (R5 * T5) * wvpBase, // -Z
    };

    for (int i = 0; i < 6; ++i)
        m_faces[i].Draw(M[i]);
}
