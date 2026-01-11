#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <dxgi.h>
#include <cassert>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace DX {
    class DeviceResources {
    public:
        DeviceResources(DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
                        DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT,
                        unsigned int backBufferCount = 2);
        void SetWindow(HWND hwnd, unsigned int width, unsigned int height);
        void CreateDeviceResources();
        void CreateWindowSizeDependentResources();
        void WindowSizeChanged(unsigned int width, unsigned int height);
        void Present();
        // DeviceResources.h  の public: に追加
        unsigned int Width()  const { return m_width; }
        unsigned int Height() const { return m_height; }

        ID3D11Device*            Dev()  const { return m_device.Get(); }
        ID3D11DeviceContext*     Ctx()  const { return m_context.Get(); }
        IDXGISwapChain*          Swap() const { return m_swapChain.Get(); }
        ID3D11RenderTargetView*  RTV()  const { return m_rtv.Get(); }
        ID3D11DepthStencilView*  DSV()  const { return m_dsv.Get(); }

    private:
        void CreateSwapChain(); 
        void CreateRenderTargets();
		HWND m_hWnd{}; // ウィンドウハンドル
		unsigned int m_width{}, m_height{};  // ウィンドウサイズ
		DXGI_FORMAT m_bbFmt{}, m_dsFmt{}; // バックバッファ・デプスフォーマット
        unsigned int m_bbCount{}; // バックバッファ数
		Microsoft::WRL::ComPtr<ID3D11Device>        m_device; // D3D11デバイス
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context; // D3D11デバイスコンテキスト
		Microsoft::WRL::ComPtr<IDXGISwapChain>      m_swapChain;// スワップチェイン
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv; // レンダーターゲットビュー
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_dsv; // デプスステンシルビュー
		Microsoft::WRL::ComPtr<ID3D11Texture2D>        m_depth; // デプステクスチャ
		Microsoft::WRL::ComPtr<IDXGIFactory>        m_factory; // DXGIファクトリ
    };
}