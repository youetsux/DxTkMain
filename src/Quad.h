#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <CommonStates.h>
#include "Texture.h"

class Quad {
public:
    Quad() = default;
    ~Quad() = default;

    // Gfx::Init() 済みが前提。引数なしでOK。
    HRESULT Initialize();

    // 呼び出し側（Appなど）で合成した WVP を渡す
    void Draw(const DirectX::XMMATRIX& wvp);

    // 顧問ステート（CommonStates）を外から渡す
    void SetCommonStates(DirectX::CommonStates* states) { m_states = states; }

    // 便利：テクスチャのサイズ
    unsigned TexWidth()  const { return m_texture.Width(); }
    unsigned TexHeight() const { return m_texture.Height(); }

private:
    // 定数バッファ（b0: 4x4）
    Microsoft::WRL::ComPtr<ID3D11Buffer>       m_cbPerObject;

    // ジオメトリ
    Microsoft::WRL::ComPtr<ID3D11Buffer>       m_vb;
    Microsoft::WRL::ComPtr<ID3D11Buffer>       m_ib;

    // シェーダ & レイアウト（Quad が責務を持つ）
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_ps;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_layout;
    // ★ Quad が所有するテクスチャ
    Texture m_texture;

    // ★ CommonStates は借り参照（所有しない）
    DirectX::CommonStates* m_states = nullptr;
};
