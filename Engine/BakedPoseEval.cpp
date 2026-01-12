#include "BakedPoseEval.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>

// StepC COMPLETE:
// Runtime transform evaluation is fully baked-data driven.
// No ufbx_scene or import-side data is referenced from this module.

using namespace DirectX;

float BakedPoseEval::NormalizeTime(const BakedAnimClip* clip, float time, bool loop)
{
    if (!clip)
    {
        return time;
    }

    const float start = clip->start_time;
    const float end = clip->end_time;
    const float duration = clip->duration;

    if (loop && duration > 0.0f)
    {
        float t = time - start;
        t = std::fmod(t, duration);
        if (t < 0.0f)
        {
            t += duration;
        }
        return start + t;
    }
    else
    {
        if (time < start) return start;
        if (time > end) return end;
        return time;
    }
}


namespace
{
    struct LocalTRS
    {
        XMVECTOR t;
        XMVECTOR r;
        XMVECTOR s;
    };

    static float Clamp01(float x)
    {
        if (x < 0.0f) return 0.0f;
        if (x > 1.0f) return 1.0f;
        return x;
    }

    static XMVECTOR LerpVec3(XMVECTOR a, XMVECTOR b, float t)
    {
        return XMVectorAdd(a, XMVectorScale(XMVectorSubtract(b, a), t));
    }

    static XMVECTOR NormalizeQuat(XMVECTOR q)
    {
        float len_sq = XMVectorGetX(XMVector4Dot(q, q));
        if (len_sq <= 0.0f) return XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        float inv_len = 1.0f / std::sqrt(len_sq);
        return XMVectorScale(q, inv_len);
    }

    static XMVECTOR SlerpQuat(XMVECTOR qa, XMVECTOR qb, float t)
    {
        qa = NormalizeQuat(qa);
        qb = NormalizeQuat(qb);

        float dot = XMVectorGetX(XMVector4Dot(qa, qb));
        if (dot < 0.0f)
        {
            qb = XMVectorNegate(qb);
            dot = -dot;
        }

        const float kEps = 1e-5f;
        if (dot > 1.0f - kEps)
        {
            XMVECTOR q = XMVectorAdd(qa, XMVectorScale(XMVectorSubtract(qb, qa), t));
            return NormalizeQuat(q);
        }

        dot = std::max(-1.0f, std::min(1.0f, dot));
        float theta = std::acos(dot);
        float sin_theta = std::sin(theta);
        if (std::abs(sin_theta) < kEps)
        {
            return qa;
        }

        float w0 = std::sin((1.0f - t) * theta) / sin_theta;
        float w1 = std::sin(t * theta) / sin_theta;
        XMVECTOR q = XMVectorAdd(XMVectorScale(qa, w0), XMVectorScale(qb, w1));
        return NormalizeQuat(q);
    }

    static XMVECTOR RestT(const BakedRigNode& n)
    {
        return XMVectorSet(n.t[0], n.t[1], n.t[2], 0.0f);
    }
    static XMVECTOR RestR(const BakedRigNode& n)
    {
        return XMVectorSet(n.r[0], n.r[1], n.r[2], n.r[3]);
    }
    static XMVECTOR RestS(const BakedRigNode& n)
    {
        return XMVectorSet(n.s[0], n.s[1], n.s[2], 0.0f);
    }

    static XMVECTOR SampleVec3Keys(const std::vector<BakedVec3Key>& keys, float time)
    {
        if (keys.empty())
        {
            return XMVectorZero();
        }

        if (time <= keys.front().time)
        {
            const BakedVec3Key& k = keys.front();
            return XMVectorSet(k.v[0], k.v[1], k.v[2], 0.0f);
        }
        if (time >= keys.back().time)
        {
            const BakedVec3Key& k = keys.back();
            return XMVectorSet(k.v[0], k.v[1], k.v[2], 0.0f);
        }

        std::size_t hi = 1;
        while (hi < keys.size() && keys[hi].time < time) { ++hi; }
        if (hi >= keys.size()) hi = keys.size() - 1;
        std::size_t lo = (hi > 0) ? (hi - 1) : 0;

        const BakedVec3Key& a = keys[lo];
        const BakedVec3Key& b = keys[hi];

        float dt = b.time - a.time;
        float u = (dt > 0.0f) ? ((time - a.time) / dt) : 0.0f;
        u = Clamp01(u);

        XMVECTOR va = XMVectorSet(a.v[0], a.v[1], a.v[2], 0.0f);
        XMVECTOR vb = XMVectorSet(b.v[0], b.v[1], b.v[2], 0.0f);
        return LerpVec3(va, vb, u);
    }

    static XMVECTOR SampleQuatKeys(const std::vector<BakedQuatKey>& keys, float time)
    {
        if (keys.empty())
        {
            return XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        }

        if (time <= keys.front().time)
        {
            const BakedQuatKey& k = keys.front();
            return XMVectorSet(k.q[0], k.q[1], k.q[2], k.q[3]);
        }
        if (time >= keys.back().time)
        {
            const BakedQuatKey& k = keys.back();
            return XMVectorSet(k.q[0], k.q[1], k.q[2], k.q[3]);
        }

        std::size_t hi = 1;
        while (hi < keys.size() && keys[hi].time < time) { ++hi; }
        if (hi >= keys.size()) hi = keys.size() - 1;
        std::size_t lo = (hi > 0) ? (hi - 1) : 0;

        const BakedQuatKey& a = keys[lo];
        const BakedQuatKey& b = keys[hi];

        float dt = b.time - a.time;
        float u = (dt > 0.0f) ? ((time - a.time) / dt) : 0.0f;
        u = Clamp01(u);

        XMVECTOR qa = XMVectorSet(a.q[0], a.q[1], a.q[2], a.q[3]);
        XMVECTOR qb = XMVectorSet(b.q[0], b.q[1], b.q[2], b.q[3]);
        return SlerpQuat(qa, qb, u);
    }

    static bool FindChannelByNodeIndex(const BakedAnimClip* clip, uint32_t node_index, const BakedAnimChannel** out_ch)
    {
        if (!clip) return false;
        for (const BakedAnimChannel& ch : clip->channels)
        {
            if (ch.node_index == node_index)
            {
                *out_ch = &ch;
                return true;
            }
        }
        return false;
    }

    static LocalTRS EvalLocalTRS(const BakedRig& rig, const BakedAnimClip* clip, uint32_t node_index, float time)
    {
        LocalTRS out{};

        const BakedRigNode& rest = rig.nodes[node_index];
        out.t = RestT(rest);
        out.r = RestR(rest);
        out.s = RestS(rest);

        const BakedAnimChannel* ch = nullptr;
        if (FindChannelByNodeIndex(clip, node_index, &ch))
        {
            if (!ch->translation_keys.empty())
            {
                out.t = SampleVec3Keys(ch->translation_keys, time);
            }
            if (!ch->rotation_keys.empty())
            {
                out.r = SampleQuatKeys(ch->rotation_keys, time);
            }
            if (!ch->scale_keys.empty())
            {
                out.s = SampleVec3Keys(ch->scale_keys, time);
            }
        }

        return out;
    }

    static XMMATRIX ComposeLocalMatrix(const LocalTRS& l)
    {
        XMMATRIX s = XMMatrixScalingFromVector(l.s);
        XMMATRIX r = XMMatrixRotationQuaternion(l.r);
        XMMATRIX t = XMMatrixTranslationFromVector(l.t);
        return s * r * t;
    }

    static void BuildWorldFromLocal(const BakedRig& rig,
                                   const std::vector<XMMATRIX>& local,
                                   std::vector<XMMATRIX>& out_world)
    {
        const std::size_t n = rig.nodes.size();
        out_world.resize(n);

        enum : uint8_t
        {
            kUnvisited = 0,
            kVisiting = 1,
            kDone = 2,
        };

        std::vector<uint8_t> state(n, kUnvisited);
        std::vector<uint8_t> in_cycle(n, 0);

        auto eval_node = [&](auto&& self, std::size_t i) -> void
        {
            if (i >= n) return;
            if (state[i] == kDone) return;
            if (state[i] == kVisiting)
            {
                // Cycle detected: fall back to local.
                out_world[i] = local[i];
                state[i] = kDone;
                in_cycle[i] = 1;
                return;
            }

            state[i] = kVisiting;

            const uint32_t parent = rig.nodes[i].parent;
            if (parent == 0xFFFFFFFFu || (std::size_t)parent >= n)
            {
                out_world[i] = local[i];
            }
            else
            {
                self(self, (std::size_t)parent);
                if (in_cycle[parent])
                {
                    out_world[i] = local[i];
                    in_cycle[i] = 1;
                }
                else
                {
                    out_world[i] = local[i] * out_world[parent];
                }
            }

            state[i] = kDone;
        };

        for (std::size_t i = 0; i < n; ++i)
        {
            eval_node(eval_node, i);
        }
    }
}


void BakedPoseEval::EvaluateNodeLocal(const BakedRig& rig,
                                     const BakedAnimClip* clip,
                                     float time,
                                     std::vector<XMMATRIX>& out_node_local)
{
    const std::size_t n = rig.nodes.size();
    out_node_local.resize(n);

    for (std::size_t i = 0; i < n; ++i)
    {
        LocalTRS l = EvalLocalTRS(rig, clip, (uint32_t)i, time);
        out_node_local[i] = ComposeLocalMatrix(l);
    }
}

void BakedPoseEval::EvaluateNodeWorld(const BakedRig& rig,
                                     const BakedAnimClip* clip,
                                     float time,
                                     std::vector<XMMATRIX>& out_node_world)
{
    std::vector<XMMATRIX> local;
    EvaluateNodeLocal(rig, clip, time, local);

    BuildWorldFromLocal(rig, local, out_node_world);
}


BakedPosePlayer::BakedPosePlayer()
    : rig_(nullptr)
    , clip_(nullptr)
    , loop_(false)
    , build_palette_on_evaluate_(false)
{
}

void BakedPosePlayer::SetRig(const BakedRig* rig)
{
    rig_ = rig;
}

void BakedPosePlayer::SetClip(const BakedAnimClip* clip)
{
    clip_ = clip;
}

void BakedPosePlayer::SetLoop(bool loop)
{
    loop_ = loop;
}

void BakedPosePlayer::SetBuildPaletteOnEvaluate(bool enable)
{
    build_palette_on_evaluate_ = enable;
}

void BakedPosePlayer::Evaluate(float time)
{
    if (!rig_)
    {
        local_.node_local.clear();
        world_.node_world.clear();
        palette_.matrices.clear();
        return;
    }

    const float t = BakedPoseEval::NormalizeTime(clip_, time, loop_);
    BakedPoseEval::EvaluateNodeLocal(*rig_, clip_, t, local_.node_local);

    BuildWorldFromLocal(*rig_, local_.node_local, world_.node_world);

    if (build_palette_on_evaluate_)
    {
        BuildSkinPalette();
    }
}

const BakedPoseLocal& BakedPosePlayer::GetLocalPose() const
{
    return local_;
}

const BakedPoseWorld& BakedPosePlayer::GetWorldPose() const
{
    return world_;
}


void BakedPoseEval::BuildSkinPaletteStub(const BakedPoseWorld& world,
                                  BakedSkinPalette& out_palette)
{
    out_palette.matrices = world.node_world;
}

void BakedPosePlayer::BuildSkinPalette()
{
    BakedPoseEval::BuildSkinPaletteStub(world_, palette_);
}

const BakedSkinPalette& BakedPosePlayer::GetSkinPalette() const
{
    return palette_;
}

void BakedPosePlayer::EvaluateWithPalette(float time)
{
    const bool prev = build_palette_on_evaluate_;
    build_palette_on_evaluate_ = true;
    Evaluate(time);
    build_palette_on_evaluate_ = prev;
}
