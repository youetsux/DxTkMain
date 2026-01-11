#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cassert>
#include "Gfx.h"

namespace {
    DX::DeviceResources* g_dr = nullptr; // ★ 非所有の生ポインタ
}

void Gfx::Init(DX::DeviceResources* dr)
{
    g_dr = dr;
}

void Gfx::Reset()
{
    g_dr = nullptr;
}

bool Gfx::IsReady()
{
    return g_dr != nullptr;
}

DX::DeviceResources* Gfx::Raw()
{
    return g_dr;
}

ID3D11Device* Gfx::Dev()
{
    assert(g_dr && "Gfx::Init() has not been called yet!");
    return g_dr->Dev();
}

ID3D11DeviceContext* Gfx::Ctx()
{
    assert(g_dr && "Gfx::Init() has not been called yet!");
    return g_dr->Ctx();
}

ID3D11RenderTargetView* Gfx::RTV()
{
    assert(g_dr);
    return g_dr->RTV();
}

ID3D11DepthStencilView* Gfx::DSV()
{
    assert(g_dr);
    return g_dr->DSV();
}

unsigned Gfx::Width()
{
    assert(g_dr);
    return g_dr->Width();
}

unsigned Gfx::Height()
{
    assert(g_dr);
    return g_dr->Height();
}

void Gfx::WindowSizeChanged(unsigned w, unsigned h)
{
    if (!g_dr || w == 0 || h == 0) return;
    g_dr->WindowSizeChanged(w, h);
}

//結論　生ポインタは気持ちいい