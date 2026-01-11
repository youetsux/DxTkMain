#pragma once
#include <string>

//-----------------------------------------------------------
// Sound（最小）
// - DirectXTK AudioEngine を内部に隠蔽
// - ファイルパス文字列で SE/BGM を鳴らす
//
// 使い方：
//   Sound::Initialize();
//   毎フレーム Sound::Update();
//   Sound::PlaySE("Assets/Sounds/jump.wav");
//   Sound::PlayBGM("Assets/Sounds/bgm.wav", true);
//-----------------------------------------------------------
namespace Sound
{
    void Initialize();
    void Update();
    void Shutdown();

    // --- 再生 ---
    void PlaySE(const std::string& filePath, float volume = 1.0f, float pitch = 0.0f, float pan = 0.0f);
    void PlayBGM(const std::string& filePath, bool loop = true, float volume = 1.0f);
    void StopBGM();

    // --- 音量 ---
    void SetMasterVolume(float volume01);
    float GetMasterVolume();

    void SetSEVolume(float volume01);
    float GetSEVolume();

    void SetBGMVolume(float volume01);
    float GetBGMVolume();

    // --- 省電力/フォーカス対応（必要なら App の WM_ACTIVATE 等から呼ぶ） ---
    void Suspend();
    void Resume();

    // 状態
    bool Ready();
}
