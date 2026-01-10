#pragma once

#include <vector>
#include <DirectXMath.h>

struct BakedPoseLocal
{
    std::vector<DirectX::XMMATRIX> node_local;
};

struct BakedPoseWorld
{
    std::vector<DirectX::XMMATRIX> node_world;
};

struct BakedSkinPalette
{
    std::vector<DirectX::XMMATRIX> matrices;
};
