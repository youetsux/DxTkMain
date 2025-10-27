#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <DirectXMath.h>

class Quad {
public:
    // Gfx::Init() 済みが前提。引数なしでOK。
    void Initialize();

    // 呼び出し側（Appなど）で合成した WVP を渡す
    void Draw(const DirectX::XMMATRIX& wvp);

    // 必要なら解放（省略可：ComPtrが自動解放）
    void Release() { *this = Quad{}; }

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
};
