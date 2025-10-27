#pragma once
#include <wrl/client.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <VertexTypes.h>
#include <Effects.h>
#include <CommonStates.h>
#include "Texture.h"

class Quad
{
public:
    Quad() = default;
    ~Quad() = default;

    Quad(const Quad&) = delete;
    Quad& operator=(const Quad&) = delete;

    HRESULT Initialize();
    void Draw(const DirectX::XMMATRIX& wvp);

    // 互換性のために残すが、内部所有に切替後は無視してOK
    //void SetCommonStates(DirectX::CommonStates* /*states*/) {}

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer>       m_vb;
    Microsoft::WRL::ComPtr<ID3D11Buffer>       m_ib;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_inputLayout;
    std::unique_ptr<DirectX::BasicEffect>      m_effect;

    // ★ Quad が自前で所有
    std::unique_ptr<DirectX::CommonStates>     m_states;

    Texture m_texture;
};
