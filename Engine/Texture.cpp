#include "Texture.h"
#include <Windows.h>                // MultiByteToWideChar
#include <WICTextureLoader.h>       // ★ DirectXTK
#include "Gfx.h"                    // Gfx::Dev()

using Microsoft::WRL::ComPtr;

Texture::Texture() {}
Texture::~Texture() { Release(); }

// UTF-8 → UTF-16（C++17/Win専用・簡潔）
std::wstring Texture::ToWString(const std::string& s)
{
    if (s.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), len);
    return w;
}

HRESULT Texture::Load(std::string fileName)
{
    Release();

    auto* device = Gfx::Dev();
    if (!device) return E_POINTER;

    const std::wstring wpath = ToWString(fileName);

    // DXTK 1行読み込み（WIC対応フォーマット）＋ SRV 作成
    ComPtr<ID3D11Resource> tex;
    ComPtr<ID3D11ShaderResourceView> srv;

    // 既定: WIC_LOADER_DEFAULT（必要なら FORCE_SRGB 等に変更可）
    HRESULT hr = DirectX::CreateWICTextureFromFileEx(
        device,
        wpath.c_str(),
        0,                              // maxsize = 既定
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0,                              // CPUアクセスなし
        0,                              // misc flags なし
        DirectX::WIC_LOADER_DEFAULT,    // ローダーフラグ
        reinterpret_cast<ID3D11Resource**>(tex.ReleaseAndGetAddressOf()),
        srv.ReleaseAndGetAddressOf()
    );
    if (FAILED(hr)) return hr;

    // 幅・高さを取得（2Dテクスチャ前提）
    m_width = m_height = 0;
    if (tex)
    {
        ComPtr<ID3D11Texture2D> t2d;
        if (SUCCEEDED(tex.As(&t2d)))
        {
            D3D11_TEXTURE2D_DESC d{};
            t2d->GetDesc(&d);
            m_width = d.Width;
            m_height = d.Height;
        }
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
