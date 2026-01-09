#pragma once

#include <cstdint>
#include <vector>

// Minimal baked animation container.
// Stores per-node local TRS keyframes in engine-owned format.

struct BakedVec3Key
{
    float time = 0.0f;
    float v[3] = { 0.0f, 0.0f, 0.0f };
};

struct BakedQuatKey
{
    float time = 0.0f;
    float q[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
};

struct BakedAnimChannel
{
    uint32_t node_index = 0xFFFFFFFFu;
    std::vector<BakedVec3Key> translation_keys;
    std::vector<BakedQuatKey> rotation_keys;
    std::vector<BakedVec3Key> scale_keys;
};

struct BakedAnimClip
{
    float start_time = 0.0f;
    float end_time = 0.0f;
    float duration = 0.0f;
    float sample_rate = 0.0f;

    std::vector<BakedAnimChannel> channels;
};
