#include "BakedPoseEvalTest.h"

#include "BakedPoseEval.h"

#include <Windows.h>

#include <cmath>
#include <cstddef>
#include <vector>


#ifndef BAKED_POSE_EVAL_SELFTESTS
#define BAKED_POSE_EVAL_SELFTESTS 0
#endif
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

#if BAKED_POSE_EVAL_SELFTESTS
        SelfTest_WorldOrderIndependent();
        SelfTest_PartialChannelTRS();
#endif
    }

    void SelfTest_WorldOrderIndependent()
    {
        // Rig layout intentionally places a child (index 0) before its parent (index 1).
        // Old order-dependent implementations would compute node[0] world before node[1].
        BakedRig rig;
        rig.nodes.resize(2);

        // node[1] is root
        rig.nodes[1].parent = 0xFFFFFFFFu;
        rig.nodes[1].t[0] = 10.0f;
        rig.nodes[1].t[1] = 0.0f;
        rig.nodes[1].t[2] = 0.0f;

        // node[0] is child of node[1]
        rig.nodes[0].parent = 1u;
        rig.nodes[0].t[0] = 1.0f;
        rig.nodes[0].t[1] = 0.0f;
        rig.nodes[0].t[2] = 0.0f;

        std::vector<XMMATRIX> world;
        BakedPoseEval::EvaluateNodeWorld(rig, nullptr, 0.0f, world);

        if (world.size() != 2)
        {
            OutputDebugStringA("SelfTest_WorldOrderIndependent FAILED: size mismatch\n");
            return;
        }

        XMFLOAT4X4 m0;
        XMFLOAT4X4 m1;
        XMStoreFloat4x4(&m0, world[0]);
        XMStoreFloat4x4(&m1, world[1]);

        const float node1_x = m1._41;
        const float node0_x = m0._41;

        // Expect root at 10, child at 11.
        const float eps = 1e-4f;
        const bool ok = (std::abs(node1_x - 10.0f) < eps) && (std::abs(node0_x - 11.0f) < eps);
        if (!ok)
        {
            char buf[256];
            wsprintfA(buf,
                "SelfTest_WorldOrderIndependent FAILED: node1_x=%f node0_x=%f\n",
                node1_x,
                node0_x);
            OutputDebugStringA(buf);
        }
        else
        {
            OutputDebugStringA("SelfTest_WorldOrderIndependent OK\n");
        }
    }


    void SelfTest_PartialChannelTRS()
    {
        // Build a rig with non-identity rest rotation and scale.
        BakedRig rig;
        rig.nodes.resize(1);

        rig.nodes[0].parent = 0xFFFFFFFFu;
        rig.nodes[0].t[0] = 0.0f;
        rig.nodes[0].t[1] = 0.0f;
        rig.nodes[0].t[2] = 0.0f;

        // Rest rotation: 90 degrees around Z.
        // Quaternion (x,y,z,w) = (0,0,sin(45deg),cos(45deg)) = (0,0,0.70710678,0.70710678)
        rig.nodes[0].r[0] = 0.0f;
        rig.nodes[0].r[1] = 0.0f;
        rig.nodes[0].r[2] = 0.70710678f;
        rig.nodes[0].r[3] = 0.70710678f;

        // Rest scale
        rig.nodes[0].s[0] = 2.0f;
        rig.nodes[0].s[1] = 3.0f;
        rig.nodes[0].s[2] = 4.0f;

        // Clip with translation keys only (rotation/scale keys intentionally missing).
        BakedAnimClip clip;
        clip.start_time = 0.0f;
        clip.end_time = 1.0f;
        clip.duration = 1.0f;
        clip.sample_rate = 30.0f;

        clip.channels.resize(1);
        clip.channels[0].node_index = 0u;
        clip.channels[0].translation_keys.push_back(BakedVec3Key{ 0.0f, { 1.0f, 2.0f, 3.0f } });
        clip.channels[0].translation_keys.push_back(BakedVec3Key{ 1.0f, { 11.0f, 12.0f, 13.0f } });

        std::vector<XMMATRIX> node_local;
        BakedPoseEval::EvaluateNodeLocal(rig, &clip, 0.5f, node_local);

        XMVECTOR out_s = XMVectorZero();
        XMVECTOR out_r = XMQuaternionIdentity();
        XMVECTOR out_t = XMVectorZero();
        XMMatrixDecompose(&out_s, &out_r, &out_t, node_local[0]);

        // Expected: translation interpolated, rotation/scale from rest.
        const XMVECTOR exp_t = XMVectorSet(6.0f, 7.0f, 8.0f, 0.0f);
        const XMVECTOR exp_s = XMVectorSet(2.0f, 3.0f, 4.0f, 0.0f);
        const XMVECTOR exp_r = XMQuaternionNormalize(XMVectorSet(0.0f, 0.0f, 0.70710678f, 0.70710678f));

        const float eps = 1e-4f;

        const XMVECTOR dt = XMVectorAbs(XMVectorSubtract(out_t, exp_t));
        const XMVECTOR ds = XMVectorAbs(XMVectorSubtract(out_s, exp_s));

        const float dt_x = XMVectorGetX(dt);
        const float dt_y = XMVectorGetY(dt);
        const float dt_z = XMVectorGetZ(dt);

        const float ds_x = XMVectorGetX(ds);
        const float ds_y = XMVectorGetY(ds);
        const float ds_z = XMVectorGetZ(ds);

        // Quaternions can be negated and represent the same rotation.
        const XMVECTOR r_a = XMQuaternionNormalize(out_r);
        const XMVECTOR r_b = XMQuaternionNormalize(XMVectorNegate(out_r));
        const float dot_a = XMVectorGetX(XMVector4Dot(r_a, exp_r));
        const float dot_b = XMVectorGetX(XMVector4Dot(r_b, exp_r));
        const float dot = (fabsf(dot_a) > fabsf(dot_b)) ? dot_a : dot_b;

        const bool ok_t = (dt_x < eps) && (dt_y < eps) && (dt_z < eps);
        const bool ok_s = (ds_x < eps) && (ds_y < eps) && (ds_z < eps);
        const bool ok_r = fabsf(1.0f - fabsf(dot)) < 1e-3f;

        if (!ok_t || !ok_r || !ok_s)
        {
            char buf[256];
            wsprintfA(buf,
                "SelfTest_PartialChannelTRS FAILED: ok_t=%d ok_r=%d ok_s=%d\n",
                ok_t ? 1 : 0,
                ok_r ? 1 : 0,
                ok_s ? 1 : 0);
            OutputDebugStringA(buf);
        }
        else
        {
            OutputDebugStringA("SelfTest_PartialChannelTRS OK\n");
        }
    }
}
