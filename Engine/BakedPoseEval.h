#pragma once

#include "BakedRig.h"
#include "BakedAnim.h"

#include <DirectXMath.h>

#include <vector>

struct BakedPoseEval
{
    static float NormalizeTime(const BakedAnimClip* clip, float time, bool loop);

    static void EvaluateNodeLocal(const BakedRig& rig,
        const BakedAnimClip* clip,
        float time,
        std::vector<DirectX::XMMATRIX>& out_node_local);

    static void EvaluateNodeWorld(const BakedRig& rig,
        const BakedAnimClip* clip,
        float time,
        std::vector<DirectX::XMMATRIX>& out_node_world);
};
