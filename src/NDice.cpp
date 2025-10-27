#include "NDice.h"
#include <vector>
#include "Gfx.h"
#include <VertexTypes.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

//extern ComPtr<ID3D11Device>        GfxDev();
//extern ComPtr<ID3D11DeviceContext> GfxCtx();

static void PushFace(std::vector<VertexPNCT>& v, std::vector<uint16_t>& idx,
    XMFLOAT3 p0, XMFLOAT3 p1, XMFLOAT3 p2, XMFLOAT3 p3,
    XMFLOAT3 n, float u0, float v0, float u1, float v1)
{
    uint16_t base = static_cast<uint16_t>(v.size());
    v.push_back({ p0, n, { u0, v0 } });
    v.push_back({ p1, n, { u1, v0 } });
    v.push_back({ p2, n, { u1, v1 } });
    v.push_back({ p3, n, { u0, v1 } });
    const uint16_t b = static_cast<uint16_t>(base);
    const uint16_t tri[6] = { static_cast<uint16_t>(b + 0), static_cast<uint16_t>(b + 1),
                              static_cast<uint16_t>(b + 2), static_cast<uint16_t>(b + 0),
                              static_cast<uint16_t>(b + 2), static_cast<uint16_t>(b + 3) };
    idx.insert(idx.end(), std::begin(tri), std::end(tri));
}

HRESULT NDice::BuildGeometry()
{
    auto device = Gfx::Dev();

    const float s = 0.5f;
    // 頂点座標（右手系・立方体の8頂点）
    XMFLOAT3 p000{ -s,-s,-s }, p001{ -s,-s,+s }, p010{ -s,+s,-s }, p011{ -s,+s,+s };
    XMFLOAT3 p100{ +s,-s,-s }, p101{ +s,-s,+s }, p110{ +s,+s,-s }, p111{ +s,+s,+s };

    std::vector<VertexPNCT> vertices;
    std::vector<uint16_t>   indices;
    vertices.reserve(24);
    indices.reserve(36);

    // ===== 🎲 UV設定（横3 × 縦4、左右マージンあり）=====
    const float u_margin = 0.125f;                     // 左右の余白
    const float du = (1.0f - u_margin * 2.0f) / 3.0f;  // 1セル横幅 (=0.25)
    const float dv = 1.0f / 4.0f;                      // 1セル縦幅 (=0.25)

    // 展開図の位置（col,row）：
    // row=0:      [  ][ 1 ][  ]
    // row=1:      [  ][ 2 ][  ]
    // row=2:      [  ][ 6 ][  ]
    // row=3: [ 3 ][ 5 ][ 4 ]
    //
    // f: 0..5 = (+X, -X, +Y, -Y, +Z, -Z)
    struct FaceUVIndex { int col, row; };
    const FaceUVIndex uvmap[6] = {
        { 0, 3 }, // f=0  +X → 3
        { 2, 3 }, // f=1  -X → 4
        { 1, 1 }, // f=2  +Y → 2
        { 1, 3 }, // f=3  -Y → 5
        { 1, 0 }, // f=4  +Z → 1
        { 1, 2 }  // f=5  -Z → 6
    };

    for (int f = 0; f < 6; ++f)
    {
        const float u0 = u_margin + uvmap[f].col * du;
        const float u1 = u0 + du;
        const float v0 = uvmap[f].row * dv; // 反転なし（上→下）
        const float v1 = v0 + dv;

        switch (f)
        {
        case 0: // +X (3)
            // 左上, 右上, 右下, 左下 の順で面を作成（頂点の向きに注意）
            PushFace(vertices, indices, p110, p111, p101, p100, XMFLOAT3(1, 0, 0), u0, v0, u1, v1);
            break;
        case 1: // -X (4)
            PushFace(vertices, indices, p011, p010, p000, p001, XMFLOAT3(-1, 0, 0), u0, v0, u1, v1);
            break;
        case 2: // +Y (2)
            PushFace(vertices, indices, p010, p011, p111, p110, XMFLOAT3(0, 1, 0), u0, v0, u1, v1);
            break;
        case 3: // -Y (5)
            PushFace(vertices, indices, p100, p101, p001, p000, XMFLOAT3(0, -1, 0), u0, v0, u1, v1);
            break;
        case 4: // +Z (1)
            PushFace(vertices, indices, p111, p011, p001, p101, XMFLOAT3(0, 0, 1), u0, v0, u1, v1);
            break;
        case 5: // -Z (6)
            PushFace(vertices, indices, p010, p110, p100, p000, XMFLOAT3(0, 0, -1), u0, v0, u1, v1);
            break;
        }
    }

    // ===== 頂点/インデックスバッファ作成 =====
    D3D11_BUFFER_DESC vbd{};
    vbd.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(VertexPNCT));
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vsd{};
    vsd.pSysMem = vertices.data();

    HRESULT hr = device->CreateBuffer(&vbd, &vsd, m_vb.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return hr;

    D3D11_BUFFER_DESC ibd{};
    ibd.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint16_t));
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA isd{};
    isd.pSysMem = indices.data();

    hr = device->CreateBuffer(&ibd, &isd, m_ib.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return hr;

    return S_OK;
}


HRESULT NDice::Initialize()
{
    auto device = Gfx::Dev();

    m_states = std::make_unique<CommonStates>(device);

    // BasicEffect: ライティングON
    auto be = std::make_shared<BasicEffect>(device);
    be->SetLightingEnabled(true);
    
    be->SetPerPixelLighting(true);
    be->SetTextureEnabled(true);
    be->SetVertexColorEnabled(false);
    be->SetAmbientLightColor({ 0.2f, 0.2f, 0.2f });

    m_basicRaw = be.get();
    m_effect = be;

    // 入力レイアウト
    const void* vsCode = nullptr; 
    size_t vsLen = 0;
    m_effect->GetVertexShaderBytecode(&vsCode, &vsLen);

    static const D3D11_INPUT_ELEMENT_DESC s_layout[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    HRESULT hr = device->CreateInputLayout(
        DirectX::VertexPositionNormalTexture::InputElements,
        DirectX::VertexPositionNormalTexture::InputElementCount,
        vsCode, vsLen,
        m_inputLayout.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return hr;

    // 頂点構築
    hr = BuildGeometry();
    if (FAILED(hr)) return hr;

    // 照明セットアップ
    SetupDefaultLight();

    return S_OK;
}

void NDice::SetupDefaultLight(const XMFLOAT3& dir)
{
    if (!m_basicRaw) return;
    m_basicRaw->SetLightEnabled(0, true);
    m_basicRaw->SetLightDiffuseColor(0, {1,1,1,1});
    m_basicRaw->SetLightDirection(0, { dir.x, dir.y, dir.z, 1.0 });

    // 他ライトはOFF
    for (int i = 1; i < 3; ++i)
        m_basicRaw->SetLightEnabled(i, false);
}

HRESULT NDice::LoadTexture(const std::wstring& path)
{
    auto device = Gfx::Dev();
    auto context = Gfx::Ctx();

    ComPtr<ID3D11Resource> res;
    ComPtr<ID3D11ShaderResourceView> srv;

    HRESULT hr = DirectX::CreateWICTextureFromFile(device, context, path.c_str(),
        res.ReleaseAndGetAddressOf(),
        srv.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return hr;

    m_srv = srv;
    return S_OK;
}

void NDice::Draw(const XMMATRIX& world, const XMMATRIX& view, const XMMATRIX& proj)
{
    auto context =Gfx::Ctx();

    UINT stride = sizeof(VertexPNCT), offset = 0;
    ID3D11Buffer* vb = m_vb.Get();

    context->IASetInputLayout(m_inputLayout.Get());
    context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    context->IASetIndexBuffer(m_ib.Get(), DXGI_FORMAT_R16_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 不透明の状態
    context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_states->DepthDefault(), 0);

    if (m_basicRaw)
    {
        m_basicRaw->SetWorld(world);
        m_basicRaw->SetView(view);
        m_basicRaw->SetProjection(proj);
        if (m_srv) m_basicRaw->SetTexture(m_srv.Get());
        ID3D11SamplerState* samp = m_states->AnisotropicClamp();
        context->PSSetSamplers(0, 1, &samp);
        //m_basicRaw->SetSampler(m_states->AnisotropicClamp());
    }

    m_effect->Apply(context);
    context->DrawIndexed(36, 0, 0);
}
