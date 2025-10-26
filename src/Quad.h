#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <DirectXMath.h>
#pragma comment(lib, "d3dcompiler.lib")

class Quad {
public:
    Quad() = default;
    ~Quad() = default;

    void Initialize(ID3D11Device* device);
    void Draw(ID3D11DeviceContext* ctx);
    void Release();

private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_ps;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_layout;
    Microsoft::WRL::ComPtr<ID3D11Buffer>       m_vb;
    Microsoft::WRL::ComPtr<ID3D11Buffer>       m_ib;
    Microsoft::WRL::ComPtr<ID3D11Buffer>       m_cb;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_rsCullNone;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_dsOff;

    struct CBColor { DirectX::XMFLOAT4 color; };
};