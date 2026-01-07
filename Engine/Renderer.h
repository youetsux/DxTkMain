#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <memory>
#include <vector>
#include <d3d11.h>
#include <DirectXColors.h>

#include "Gfx.h"
#include "Model.h"

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

    //============================================================


    //============================================================
    void SubmitModel3D(int modelHandle, bool drawSkeleton = false);
    void Submit2D();   // placeholder
    void SubmitUI();   // placeholder


    void Execute();

private:
    //============================================================

    //============================================================
    struct ModelDrawCmd
    {
        int  handle = -1;
        bool drawSkeleton = false;
    };

    std::vector<ModelDrawCmd> m_queue3D;
    bool m_has2D = false; // placeholder
    bool m_hasUI = false; // placeholder

private:
    //DirectX::CommonStates* States() const { return m_states.get(); }
    //std::shared_ptr<DirectX::CommonStates> m_states;
    float m_clear[4]{ 250/255.0f, 250/255.0f, 210/255.0f, 1.0f };
};
