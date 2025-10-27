#pragma once
#include <DirectXMath.h>
#include <array>
#include "Quad.h"

class Dice
{
public:
    Dice() = default;
    ~Dice() = default;
    Dice(const Dice&) = delete;
    Dice& operator=(const Dice&) = delete;

    HRESULT Initialize();                             // 6面のQuad作成＋各UV設定＋テクスチャ読込
    void Draw(const DirectX::XMMATRIX& wvpBase);      // 各面のローカル行列を掛けて描く

private:
    std::array<Quad, 6> m_faces; // 0:+X,1:-X,2:+Y,3:-Y,4:+Z,5:-Z
};
