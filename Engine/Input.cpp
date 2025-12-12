#include "Input.h"
#include <memory>

namespace
{
    std::unique_ptr<DirectX::Keyboard> g_keyboard;
    DirectX::Keyboard::State g_state;
    DirectX::Keyboard::KeyboardStateTracker g_tracker;

    inline DirectX::Keyboard::Keys VkToDxKey(int vk)
    {
        if (vk < 0 || vk > 0xFF) return DirectX::Keyboard::Keys::None;
        return static_cast<DirectX::Keyboard::Keys>(vk);
    }
}

namespace Input
{
    void Initialize(HWND /*hwnd*/)
    {
        if (!g_keyboard)
        {
            g_keyboard = std::make_unique<DirectX::Keyboard>();
        }

        g_state = g_keyboard->GetState();
        g_tracker.Reset();
        g_tracker.Update(g_state);
    }

    void ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (!g_keyboard) return;

        // ★これが無いと GetState が更新されない環境がある
        g_keyboard->ProcessMessage(message, wParam, lParam);
    }

    void Update()
    {
        if (!g_keyboard) return;

        g_state = g_keyboard->GetState();
        g_tracker.Update(g_state);
    }

    // --- VK_*（int）---
    bool IsKey(int vk) { return IsKey(VkToDxKey(vk)); }
    bool IsKeyDown(int vk) { return IsKeyDown(VkToDxKey(vk)); }
    bool IsKeyUp(int vk) { return IsKeyUp(VkToDxKey(vk)); }

    // --- DXTK Keys ---
    bool IsKey(DirectX::Keyboard::Keys key)
    {
        if (!g_keyboard) return false;
        return g_state.IsKeyDown(key);
    }

    bool IsKeyDown(DirectX::Keyboard::Keys key)
    {
        if (!g_keyboard) return false;
        return g_tracker.IsKeyPressed(key);
    }

    bool IsKeyUp(DirectX::Keyboard::Keys key)
    {
        if (!g_keyboard) return false;
        return g_tracker.IsKeyReleased(key);
    }
}
