#pragma once
#include <wrl/client.h>
#include <d3d11.h>
#include <memory>
#include <DirectXMath.h>
#include <Effects.h>
#include <CommonStates.h>
#include <WICTextureLoader.h>
#include <vector>
#include <string>

// Gfx::Dev(), Gfx::Ctx() は外部で定義されている想定
// 例：
// extern Microsoft::WRL::ComPtr<ID3D11Device>        GfxDev();
// extern Microsoft::WRL::ComPtr<ID3D11DeviceContext> GfxCtx();

struct VertexPNCT
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT3 nrm;
    DirectX::XMFLOAT2 uv;
};

class NDice
{
public:
    NDice() = default;
    ~NDice() = default;

    NDice(const NDice&) = delete;
    NDice& operator=(const NDice&) = delete;

    HRESULT Initialize();
    HRESULT LoadTexture(const std::wstring& path);
    void    Draw(const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj);

    void SetPremultiplied(bool on) { m_premultiplied = on; }
    void SetupDefaultLight(const DirectX::XMFLOAT3& dir = { -0.5f, -1.0f, -0.25f });

private:


    HRESULT BuildGeometry();

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer>       m_vb;
    Microsoft::WRL::ComPtr<ID3D11Buffer>       m_ib;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_inputLayout;

    std::shared_ptr<DirectX::IEffect>          m_effect;
    DirectX::BasicEffect* m_basicRaw = nullptr; // 非所有
    std::unique_ptr<DirectX::CommonStates>     m_states;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srv;

    bool m_premultiplied = false;
};
