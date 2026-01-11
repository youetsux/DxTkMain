#include "BakedDataValidate.h"

#include "BakedRig.h"
#include "BakedAnim.h"

#include <cstdint>
#include <sstream>
#include <vector>

namespace
{
    void SetError(std::string& out_error, const char* msg)
    {
        out_error = msg ? msg : "";
    }

    void SetErrorIndex(std::string& out_error, const char* msg, uint32_t index)
    {
        std::ostringstream oss;
        oss << (msg ? msg : "error") << " index=" << index;
        out_error = oss.str();
    }
}

namespace BakedDataValidate
{
    bool ValidateRig(const BakedRig& rig, std::string& out_error)
    {
        out_error.clear();

        const uint32_t node_count = static_cast<uint32_t>(rig.nodes.size());
        for (uint32_t i = 0; i < node_count; ++i)
        {
            const uint32_t parent = rig.nodes[i].parent;
            if (parent != 0xFFFFFFFFu && parent >= node_count)
            {
                SetErrorIndex(out_error, "rig: parent out of range", i);
                return false;
            }
        }

        for (uint32_t i = 0; i < node_count; ++i)
{
    const uint32_t element_id = rig.nodes[i].element_id;
    if (element_id >= static_cast<uint32_t>(rig.element_id_to_node.size()))
    {
        SetErrorIndex(out_error, "rig: node element_id out of range", i);
        return false;
    }

    const uint32_t mapped_index = rig.element_id_to_node[element_id];
    if (mapped_index != i)
    {
        SetErrorIndex(out_error, "rig: element_id_to_node mismatch", i);
        return false;
    }
}

for (uint32_t element_id = 0; element_id < static_cast<uint32_t>(rig.element_id_to_node.size()); ++element_id)
        {
            const uint32_t node_index = rig.element_id_to_node[element_id];
            if (node_index != 0xFFFFFFFFu && node_index >= node_count)
            {
                SetErrorIndex(out_error, "rig: element_id_to_node out of range", element_id);
                return false;
            }
        }

        return true;
    }

    bool ValidateRigAndClip(const BakedRig& rig, const BakedAnimClip* clip, std::string& out_error)
    {
        out_error.clear();

        if (!ValidateRig(rig, out_error))
        {
            return false;
        }

        if (!clip)
        {
            return true;
        }

        if (clip->duration < 0.0f)
        {
            SetError(out_error, "clip: negative duration");
            return false;
        }

        if (clip->end_time < clip->start_time)
        {
            SetError(out_error, "clip: end_time < start_time");
            return false;
        }

        const uint32_t node_count = static_cast<uint32_t>(rig.nodes.size());
        std::vector<uint8_t> seen_node_channel;
seen_node_channel.resize(node_count, 0u);

for (uint32_t ci = 0; ci < static_cast<uint32_t>(clip->channels.size()); ++ci)
        {
            const BakedAnimChannel& ch = clip->channels[ci];
            if (ch.node_index == 0xFFFFFFFFu)
            {
                SetErrorIndex(out_error, "clip: channel node_index invalid", ci);
                return false;
            }
            if (ch.node_index >= node_count)
            {
                SetErrorIndex(out_error, "clip: channel node_index out of range", ci);
                return false;
            }
if (seen_node_channel[ch.node_index] != 0u)
{
    SetErrorIndex(out_error, "clip: duplicate channel node_index", ci);
    return false;
}
seen_node_channel[ch.node_index] = 1u;

        }

        return true;
    }
}
