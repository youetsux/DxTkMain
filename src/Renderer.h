#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <memory>
#include <d3d11.h>
#include <DirectXColors.h>
#include <CommonStates.h>
#include "Gfx.h"

class Renderer
{
public:
    Renderer() = default;
    ~Renderer() = default;

    void Initialize();
    void OnResize(unsigned w, unsigned h);
    void BeginFrame();
    void EndFrame();
    void Present();

private:
    std::shared_ptr<DirectX::CommonStates> m_states;
    float m_clear[4]{ 0.1f, 0.18f, 0.25f, 1.0f };
};
