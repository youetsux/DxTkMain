#pragma once

#include <cstdint>
#include <vector>

struct BakedRigNode
{
    uint32_t parent = 0xFFFFFFFFu;
    uint32_t element_id = 0;
    float t[3] = { 0.0f, 0.0f, 0.0f };
    float r[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    float s[3] = { 1.0f, 1.0f, 1.0f };
};

struct BakedRig
{
    std::vector<BakedRigNode> nodes;
    std::vector<uint32_t> element_id_to_node;
};
