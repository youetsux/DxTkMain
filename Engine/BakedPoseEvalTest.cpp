#include "BakedPoseEvalTest.h"

#include "BakedPoseEval.h"

#include <Windows.h>

#include <cstddef>
#include <vector>

using namespace DirectX;

namespace BakedPoseEvalTest
{
    void RunOnce(const BakedRig& rig, const BakedAnimClip* clip, float time)
    {
        std::vector<XMMATRIX> node_world;
        BakedPoseEval::EvaluateNodeWorld(rig, clip, time, node_world);

        const std::size_t n = node_world.size();
        const std::size_t dump_n = (n < 4) ? n : 4;

        char buf[256];
        wsprintfA(buf, "BakedPoseEvalTest::RunOnce nodes=%u time=%f\n", (unsigned)n, time);
        OutputDebugStringA(buf);

        for (std::size_t i = 0; i < dump_n; ++i)
        {
            XMFLOAT4X4 m;
            XMStoreFloat4x4(&m, node_world[i]);
            wsprintfA(buf,
                      "node[%u] m00=%f m11=%f m22=%f m30=%f m31=%f m32=%f\n",
                      (unsigned)i,
                      m._11, m._22, m._33,
                      m._41, m._42, m._43);
            OutputDebugStringA(buf);
        }
    }
}
