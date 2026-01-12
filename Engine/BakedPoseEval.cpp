#include "BakedPoseEval.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdio>

#ifndef BAKED_CHANNEL_MAP_DEBUG
// 1: baked channel の node_index が rig.nodes と噛み合っているかをログで検証する
// 0: 出さない
#define BAKED_CHANNEL_MAP_DEBUG 1
#endif

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

#if BAKED_CHANNEL_MAP_DEBUG
    // 「TT兄弟（常にrest）」の典型原因：clip.channels の node_index と rig.nodes の index 空間が一致していない。
    // ここで以下をログに出す：
    //  - channels の node_index 範囲
    //  - rig.nodes.size() に対する in-range/out-of-range channel 数
    //  - rig 側 index に対して channel がヒットする node 数
    static int s_dbgCount = 0;
    // 追加の切り分け：
    //  - channel がヒットしているのに Tポーズの場合、(A)キー値が実際に変化していない か
    //    (B)この後段(パレット/スキニング)に反映されていない。
    // まず (A) を判定するため、代表1ノードのキー範囲とサンプル値を定期的に出す。
    // プローブは "動く" channel を優先して選びたい。
    // また、複数モデル/リグを切り替えた時に probe が古い index のまま残ると
    // (例: 最初のrigでは node=1 が存在するが、次のclipでは node=1 にchannelが無い)
    // という状況になり、プローブログが出なくなる。
    // rig.nodes.size() が変わったら probe を再選定する。
    static uint32_t s_probeNode = 0xFFFFFFFFu;
    static std::size_t s_lastRigNodes = 0;
    if ((s_dbgCount++ % 120) == 0) // 約2秒に1回（60fps想定）
    {
        if (clip)
        {
            std::size_t ch_count = clip->channels.size();
            uint32_t min_idx = 0xFFFFFFFFu;
            uint32_t max_idx = 0;
            std::size_t in_range = 0;
            std::size_t out_range = 0;

            std::vector<uint8_t> has_ch(n, 0);
            for (const BakedAnimChannel& ch : clip->channels)
            {
                min_idx = std::min(min_idx, ch.node_index);
                max_idx = std::max(max_idx, ch.node_index);
                if ((std::size_t)ch.node_index < n)
                {
                    ++in_range;
                    has_ch[(std::size_t)ch.node_index] = 1;
                }
                else
                {
                    ++out_range;
                }
            }

            std::size_t hit_nodes = 0;
            for (std::size_t i = 0; i < n; ++i)
            {
                if (has_ch[i]) ++hit_nodes;
            }

            // プローブ対象ノードを決める。
            //  1) rigが変わったらリセット
            //  2) "動く" 可能性の高い channel (キー数が多い/端点が違う) を優先
            if (s_lastRigNodes != n)
            {
                s_lastRigNodes = n;
                s_probeNode = 0xFFFFFFFFu;
            }
            if (s_probeNode == 0xFFFFFFFFu)
            {
                std::size_t best_score = 0;
                for (const BakedAnimChannel& ch : clip->channels)
                {
                    if ((std::size_t)ch.node_index >= n)
                    {
                        continue;
                    }

                    // スコア: キー数の合計
                    std::size_t score = ch.translation_keys.size() + ch.rotation_keys.size() + ch.scale_keys.size();

                    // 端点が同じキー列は"動かない"可能性が高いので減点
                    auto same_vec3 = [](const BakedVec3Key& a, const BakedVec3Key& b) -> bool
                        {
                            const float dx = a.v[0] - b.v[0];
                            const float dy = a.v[1] - b.v[1];
                            const float dz = a.v[2] - b.v[2];
                            const float eps = 1e-6f;
                            return (std::abs(dx) < eps && std::abs(dy) < eps && std::abs(dz) < eps);
                        };
                    auto same_quat = [](const BakedQuatKey& a, const BakedQuatKey& b) -> bool
                        {
                            const float dx = a.q[0] - b.q[0];
                            const float dy = a.q[1] - b.q[1];
                            const float dz = a.q[2] - b.q[2];
                            const float dw = a.q[3] - b.q[3];
                            const float eps = 1e-6f;
                            return (std::abs(dx) < eps && std::abs(dy) < eps && std::abs(dz) < eps && std::abs(dw) < eps);
                        };

                    if (ch.translation_keys.size() >= 2 && same_vec3(ch.translation_keys.front(), ch.translation_keys.back()))
                    {
                        if (score > 0) score -= 1;
                    }
                    if (ch.rotation_keys.size() >= 2 && same_quat(ch.rotation_keys.front(), ch.rotation_keys.back()))
                    {
                        if (score > 0) score -= 1;
                    }
                    if (ch.scale_keys.size() >= 2 && same_vec3(ch.scale_keys.front(), ch.scale_keys.back()))
                    {
                        if (score > 0) score -= 1;
                    }

                    if (score > best_score)
                    {
                        best_score = score;
                        s_probeNode = ch.node_index;
                    }
                }

                // どうしても選べなかった場合は最初のin-range channel
                if (s_probeNode == 0xFFFFFFFFu)
                {
                    for (const BakedAnimChannel& ch : clip->channels)
                    {
                        if ((std::size_t)ch.node_index < n)
                        {
                            s_probeNode = ch.node_index;
                            break;
                        }
                    }
                }
            }

#if defined(_WIN32)
            char buf[256];
            sprintf_s(buf,
                "[BakedChMap] t=%.6f rigNodes=%zu ch=%zu idx=[%u..%u] inRange=%zu outRange=%zu hitNodes=%zu\n",
                (double)time,
                n,
                ch_count,
                (unsigned)min_idx,
                (unsigned)max_idx,
                in_range,
                out_range,
                hit_nodes);
            OutputDebugStringA(buf);
#endif

            // 代表ノードのキー範囲とサンプル値を出す。
            // probe が clip 内に無い場合（スタック/モデル切替など）は再選定。
            if (s_probeNode != 0xFFFFFFFFu && (std::size_t)s_probeNode < n)
            {
                const BakedAnimChannel* probe = nullptr;
                if (!FindChannelByNodeIndex(clip, s_probeNode, &probe) || !probe)
                {
                    s_probeNode = 0xFFFFFFFFu;
                }
            }
            if (s_probeNode != 0xFFFFFFFFu && (std::size_t)s_probeNode < n)
            {
                const BakedAnimChannel* probe = nullptr;
                if (FindChannelByNodeIndex(clip, s_probeNode, &probe) && probe)
                {
                    const BakedRigNode& rest = rig.nodes[(std::size_t)s_probeNode];
                    XMVECTOR rt = RestT(rest);
                    XMVECTOR rr = RestR(rest);
                    XMVECTOR rs = RestS(rest);

                    XMVECTOR st = rt;
                    XMVECTOR sr = rr;
                    XMVECTOR ss = rs;
                    if (!probe->translation_keys.empty()) st = SampleVec3Keys(probe->translation_keys, time);
                    if (!probe->rotation_keys.empty())    sr = SampleQuatKeys(probe->rotation_keys, time);
                    if (!probe->scale_keys.empty())       ss = SampleVec3Keys(probe->scale_keys, time);

                    float rtx = XMVectorGetX(rt), rty = XMVectorGetY(rt), rtz = XMVectorGetZ(rt);
                    float stx = XMVectorGetX(st), sty = XMVectorGetY(st), stz = XMVectorGetZ(st);

                    // 回転は差分だけ（w含む）を簡易表示。
                    float rrx = XMVectorGetX(rr), rry = XMVectorGetY(rr), rrz = XMVectorGetZ(rr), rrw = XMVectorGetW(rr);
                    float srx = XMVectorGetX(sr), sry = XMVectorGetY(sr), srz = XMVectorGetZ(sr), srw = XMVectorGetW(sr);

                    double t0 = 0.0, t1 = 0.0, r0 = 0.0, r1 = 0.0, s0 = 0.0, s1 = 0.0;
                    if (!probe->translation_keys.empty()) { t0 = (double)probe->translation_keys.front().time; t1 = (double)probe->translation_keys.back().time; }
                    if (!probe->rotation_keys.empty()) { r0 = (double)probe->rotation_keys.front().time;    r1 = (double)probe->rotation_keys.back().time; }
                    if (!probe->scale_keys.empty()) { s0 = (double)probe->scale_keys.front().time;       s1 = (double)probe->scale_keys.back().time; }

#if defined(_WIN32)
                    char buf2[512];
                    sprintf_s(buf2,
                        "[BakedProbe] node=%u t=%.6f keys(T/R/S)=%zu/%zu/%zu timeRange(T/R/S)=[%.6f..%.6f]/[%.6f..%.6f]/[%.6f..%.6f] restT=(%.3f,%.3f,%.3f) sampT=(%.3f,%.3f,%.3f) restQ=(%.3f,%.3f,%.3f,%.3f) sampQ=(%.3f,%.3f,%.3f,%.3f)\n",
                        (unsigned)s_probeNode,
                        (double)time,
                        probe->translation_keys.size(),
                        probe->rotation_keys.size(),
                        probe->scale_keys.size(),
                        t0, t1,
                        r0, r1,
                        s0, s1,
                        (double)rtx, (double)rty, (double)rtz,
                        (double)stx, (double)sty, (double)stz,
                        (double)rrx, (double)rry, (double)rrz, (double)rrw,
                        (double)srx, (double)sry, (double)srz, (double)srw);
                    OutputDebugStringA(buf2);
#endif

                }
            }
        }
        else
        {
#if defined(_WIN32)
            OutputDebugStringA("[BakedChMap] clip=null\n");
#endif
        }
    }
#endif

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
