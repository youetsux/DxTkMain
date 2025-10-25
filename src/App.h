#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "Renderer.h"

class App {
public:
    void Initialize(HWND hwnd, unsigned w, unsigned h);
    void OnResize(unsigned w, unsigned h);
    void Update();
    void Render();
private:
    Renderer m_renderer;
};
