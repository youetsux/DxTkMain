#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <memory>
#include "DeviceResources.h"
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <CommonStates.h>
#include "Quad.h"


// Granular renderer (Begin/End/Present split)
class Renderer {
public:
    Renderer() = default;
    ~Renderer(); // © éŒ¾‚Ì‚Ý
    void Initialize(HWND hwnd, unsigned w, unsigned h);
    void OnResize(unsigned w, unsigned h);
    void SetClear(float r,float g,float b,float a=1.0f) { m_clear[0]=r; m_clear[1]=g; m_clear[2]=b; m_clear[3]=a; }

    void BeginFrame();  // bind & clear
    void EndFrame();    // post-frame hook
    void Present();     // swap

    // Accessors
    ID3D11Device*        Dev() const { return m_dev->Dev(); }
    ID3D11DeviceContext* Ctx() const { return m_dev->Ctx(); }
    DirectX::SpriteBatch*  Sprite() const { return m_sprite.get(); }
    DirectX::CommonStates*  States() const { return m_states.get(); }

private:
    std::unique_ptr<DX::DeviceResources> m_dev;
    float m_clear[4]{0.1f,0.18f,0.25f,1.0f};
    std::unique_ptr<DirectX::SpriteBatch>  m_sprite;
    std::unique_ptr<DirectX::CommonStates> m_states;

    // Demo drawable
    std::unique_ptr<Quad> m_quad;
};

struct FrameScope { Renderer& r; FrameScope(Renderer& rr):r(rr){ r.BeginFrame(); } ~FrameScope(){ r.EndFrame(); } };