#pragma once
#include <string>

//-----------------------------------------------------------



//

//   Sound::Initialize();

//   Sound::PlaySE("Assets/Sounds/jump.wav");
//   Sound::PlayBGM("Assets/Sounds/bgm.wav", true);
//-----------------------------------------------------------
namespace Sound
{
    void Initialize();
    void Update();
    void Shutdown();


    void PlaySE(const std::string& filePath, float volume = 1.0f, float pitch = 0.0f, float pan = 0.0f);
    void PlayBGM(const std::string& filePath, bool loop = true, float volume = 1.0f);
    void StopBGM();


    void SetMasterVolume(float volume01);
    float GetMasterVolume();

    void SetSEVolume(float volume01);
    float GetSEVolume();

    void SetBGMVolume(float volume01);
    float GetBGMVolume();


    void Suspend();
    void Resume();


    bool Ready();
}
