#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <cassert>
#include <wrl/client.h>
#include "DeviceResources.h"

using Microsoft::WRL::ComPtr;
using namespace DX;

DeviceResources::DeviceResources(DXGI_FORMAT bb, DXGI_FORMAT ds, unsigned int count)
    : m_bbFmt(bb), m_dsFmt(ds), m_bbCount(count) {}

void DeviceResources::SetWindow(HWND hwnd, unsigned int w, unsigned int h)
{ m_hWnd = hwnd; m_width = w; m_height = h; }

void DeviceResources::CreateDeviceResources()
{
    UINT flags = 0;
#if defined(_DEBUG)
    //flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL created = {};
    HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)m_factory.GetAddressOf());
    assert(SUCCEEDED(hr));
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        flags, levels, _countof(levels), D3D11_SDK_VERSION,
        m_device.GetAddressOf(), &created, m_context.GetAddressOf());
    assert(SUCCEEDED(hr));
    CreateSwapChain();
}

void DeviceResources::CreateSwapChain()
{
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = m_bbCount;
    sd.BufferDesc.Width = m_width; sd.BufferDesc.Height = m_height;
    sd.BufferDesc.Format = m_bbFmt;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    HRESULT hr = m_factory->CreateSwapChain(m_device.Get(), &sd, m_swapChain.GetAddressOf());
    assert(SUCCEEDED(hr));
    m_factory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER);
}

void DeviceResources::CreateRenderTargets()
{
    ComPtr<ID3D11Texture2D> back;
    HRESULT hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)back.GetAddressOf());
    assert(SUCCEEDED(hr));
    m_rtv.Reset();
    hr = m_device->CreateRenderTargetView(back.Get(), nullptr, m_rtv.GetAddressOf());
    assert(SUCCEEDED(hr));

    m_dsv.Reset(); m_depth.Reset();
    if (m_dsFmt != DXGI_FORMAT_UNKNOWN) {
        D3D11_TEXTURE2D_DESC td{};
        td.Width = m_width; td.Height = m_height;
        td.MipLevels = 1; td.ArraySize = 1;
        td.Format = m_dsFmt; td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        hr = m_device->CreateTexture2D(&td, nullptr, m_depth.GetAddressOf());
        assert(SUCCEEDED(hr));
        hr = m_device->CreateDepthStencilView(m_depth.Get(), nullptr, m_dsv.GetAddressOf());
        assert(SUCCEEDED(hr));
    }

    ID3D11RenderTargetView* rtvs[] = { m_rtv.Get() };
    m_context->OMSetRenderTargets(1, rtvs, m_dsv.Get());
    D3D11_VIEWPORT vp{}; vp.TopLeftX=0; vp.TopLeftY=0; vp.Width=(FLOAT)m_width; vp.Height=(FLOAT)m_height; vp.MinDepth=0; vp.MaxDepth=1;
    m_context->RSSetViewports(1, &vp);
}

void DeviceResources::CreateWindowSizeDependentResources()
{
    if (!m_swapChain) return;
    m_rtv.Reset(); m_dsv.Reset(); m_depth.Reset();
    m_swapChain->ResizeBuffers(m_bbCount, m_width, m_height, m_bbFmt, 0);
    CreateRenderTargets();
}

void DeviceResources::WindowSizeChanged(unsigned int w, unsigned int h)
{
    if (w==0 || h==0) return;
    m_width=w; m_height=h;
    CreateWindowSizeDependentResources();
}

void DeviceResources::Present()
{
    m_swapChain->Present(1, 0);
}