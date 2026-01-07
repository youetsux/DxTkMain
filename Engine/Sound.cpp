#include "Sound.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <Audio.h>   // DirectXTK
#include <memory>
#include <unordered_map>
#include <algorithm>
#include <string>

namespace
{
    std::unique_ptr<DirectX::AudioEngine> g_audio;


    std::unordered_map<std::string, std::unique_ptr<DirectX::SoundEffect>> g_seCache;


    std::unique_ptr<DirectX::SoundEffect> g_bgmEffect;
    std::unique_ptr<DirectX::SoundEffectInstance> g_bgmInst;
    std::string g_bgmPath;

    float g_masterVol = 1.0f;
    float g_seVol = 1.0f;
    float g_bgmVol = 1.0f;

    float Clamp01(float v)
    {
        return std::max(0.0f, std::min(1.0f, v));
    }



    std::wstring ToWidePath(const std::string& s)
    {
        if (s.empty()) return std::wstring();


        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (len > 0)
        {
            std::wstring w;
            w.resize((size_t)len - 1);
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
            return w;
        }


        len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, nullptr, 0);
        if (len > 0)
        {
            std::wstring w;
            w.resize((size_t)len - 1);
            MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, &w[0], len);
            return w;
        }

        return std::wstring();
    }

    DirectX::SoundEffect* GetOrLoadSE(const std::string& filePath)
    {
        auto it = g_seCache.find(filePath);
        if (it != g_seCache.end())
        {
            return it->second.get();
        }

        std::wstring wpath = ToWidePath(filePath);
        if (wpath.empty()) return nullptr;


        auto se = std::make_unique<DirectX::SoundEffect>(g_audio.get(), wpath.c_str());
        auto* ptr = se.get();
        g_seCache.emplace(filePath, std::move(se));
        return ptr;
    }

    void EnsureAudio()
    {
        if (g_audio) return;

        g_audio = std::make_unique<DirectX::AudioEngine>();
        g_audio->SetMasterVolume(g_masterVol);
    }
}

namespace Sound
{
    void Initialize()
    {
        EnsureAudio();
    }

    void Update()
    {
        if (!g_audio) return;

        if (!g_audio->Update())
        {

            g_audio.reset();

            g_seCache.clear();
            g_bgmInst.reset();
            g_bgmEffect.reset();
            g_bgmPath.clear();

            EnsureAudio();
        }
    }

    void Shutdown()
    {
        StopBGM();

        g_seCache.clear();
        g_audio.reset();
    }

    void PlaySE(const std::string& filePath, float volume, float pitch, float pan)
    {
        if (filePath.empty()) return;
        EnsureAudio();
        if (!g_audio) return;

        auto* se = GetOrLoadSE(filePath);
        if (!se) return;

        const float v = Clamp01(volume) * Clamp01(g_seVol);
        se->Play(v, pitch, pan);
    }

    void PlayBGM(const std::string& filePath, bool loop, float volume)
    {
        if (filePath.empty()) return;
        EnsureAudio();
        if (!g_audio) return;

        const float v = Clamp01(volume) * Clamp01(g_bgmVol);


        if (g_bgmInst && g_bgmPath == filePath)
        {
            g_bgmInst->SetVolume(v);


            g_bgmInst->Stop(true);
            g_bgmInst->Play(loop);
            return;
        }


        StopBGM();

        std::wstring wpath = ToWidePath(filePath);
        if (wpath.empty()) return;

        g_bgmEffect = std::make_unique<DirectX::SoundEffect>(g_audio.get(), wpath.c_str());
        g_bgmInst = g_bgmEffect->CreateInstance();
        g_bgmInst->SetVolume(v);
        g_bgmInst->Play(loop);

        g_bgmPath = filePath;
    }

    void StopBGM()
    {
        if (g_bgmInst)
        {
            g_bgmInst->Stop(true);
            g_bgmInst.reset();
        }
        g_bgmEffect.reset();
        g_bgmPath.clear();
    }

    void SetMasterVolume(float volume01)
    {
        g_masterVol = Clamp01(volume01);
        if (g_audio)
        {
            g_audio->SetMasterVolume(g_masterVol);
        }
    }

    float GetMasterVolume()
    {
        return g_masterVol;
    }

    void SetSEVolume(float volume01)
    {
        g_seVol = Clamp01(volume01);
    }

    float GetSEVolume()
    {
        return g_seVol;
    }

    void SetBGMVolume(float volume01)
    {
        g_bgmVol = Clamp01(volume01);

        if (g_bgmInst)
        {
            g_bgmInst->SetVolume(g_bgmVol);
        }
    }

    float GetBGMVolume()
    {
        return g_bgmVol;
    }

    void Suspend()
    {
        if (g_audio) g_audio->Suspend();
    }

    void Resume()
    {
        if (g_audio) g_audio->Resume();
    }

    bool Ready()
    {
        return (g_audio != nullptr);
    }
}
