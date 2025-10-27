#pragma once
#include <string>
#include <wrl/client.h>
#include <d3d11.h>

class Texture
{
public:
    Texture();
    ~Texture();

    // WIC対応フォーマットのみ（PNG/JPG/BMP/GIF/TIFF/ICO など）
    HRESULT Load(std::string fileName);
    void    Release();

    ID3D11ShaderResourceView* GetSRV() const { return m_srv.Get(); }
    ID3D11Resource* GetResource() const { return m_tex.Get(); }
    unsigned Width()  const { return m_width; }
    unsigned Height() const { return m_height; }

private:
    static std::wstring ToWString(const std::string& s); // UTF-8 -> UTF-16

private:
    Microsoft::WRL::ComPtr<ID3D11Resource>           m_tex;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srv;
    unsigned m_width = 0;
    unsigned m_height = 0;
};
