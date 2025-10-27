#include "Texture.h"

// 重要: codecvt は使わず Windows API で文字コード変換
#include <Windows.h>

#include <DirectXTex.h>   // DirectXTex（WIC読み込みで使用）
#include "Gfx.h"          // Gfx::Dev(), Gfx::Ctx() を想定

using Microsoft::WRL::ComPtr;
using namespace DirectX;

Texture::Texture() {}
Texture::~Texture() { Release(); }

// UTF-8 → UTF-16（C++17対応：MultiByteToWideChar 使用）
std::wstring Texture::ToWString(const std::string& s)
{
    if (s.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (len <= 0) return std::wstring();
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), len);
    return w;
}

HRESULT Texture::Load(std::string fileName)
{
    Release();

    auto* device = Gfx::Dev();
    auto* context = Gfx::Ctx();
    if (!device || !context)
        return E_POINTER;

    const std::wstring wpath = ToWString(fileName);

    // ---- WIC対応フォーマットを読み込み（ミップなし / sRGB変換なし）----
    ScratchImage image;
    HRESULT hr = LoadFromWICFile(wpath.c_str(), WIC_FLAGS_NONE, nullptr, image);
    if (FAILED(hr))
        return hr;

    const TexMetadata& meta = image.GetMetadata();

    // ---- GPU テクスチャ作成 ----
    ComPtr<ID3D11Resource> tex;
    hr = CreateTexture(device,
        image.GetImages(),
        image.GetImageCount(),
        meta,
        tex.GetAddressOf());
    if (FAILED(hr))
        return hr;

    // ---- SRV 作成（※ D3D11 のメソッドを呼ぶこと！）----
    ComPtr<ID3D11ShaderResourceView> srv;
    hr = device->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf());
    if (FAILED(hr))
        return hr;

    if (meta.dimension == TEX_DIMENSION_TEXTURE2D)
    {
        m_width = static_cast<unsigned>(meta.width);
        m_height = static_cast<unsigned>(meta.height);
    }

    m_tex = tex;
    m_srv = srv;
    return S_OK;
}

void Texture::Release()
{
    m_srv.Reset();
    m_tex.Reset();
    m_width = 0;
    m_height = 0;
}
