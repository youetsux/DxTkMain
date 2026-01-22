#include "Model.h"
#include "FbxModel.h"
#include "Camera.h"
#include "Input.h"
#include "EngineTime.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <vector>
#include <string>
#include <unordered_map>
#include <cstring>
#include <utility>

//#define DBG_LOG(fmt, ...)                          \
//do {                                               \
//    char _buf[512];                                \
//    std::snprintf(_buf, sizeof(_buf), fmt, __VA_ARGS__); \
//    OutputDebugStringA(_buf);                      \
//} while (0)

using namespace DirectX;

namespace
{
    struct AnimState
    {
        int    startFrame = 0;
        int    endFrame = 0;
        float  speed = 0.0f;
        float  currentFrame = 0.0f;

        int    stackIndex = -1;
        double timeSec = 0.0;

        bool   paused = false;
        bool   loop = true;
    };

    struct ModelData
    {
        FbxModel* pFbx = nullptr;         // shared (no ownership)
        Transform* pTransform = nullptr;   // external (no ownership)
        std::string fileName;

        AnimState   anim;
        bool        inUse = false;

        // Root
        float        rootScale = 1.0f;
        DirectX::XMFLOAT4 rootRotationQ = DirectX::XMFLOAT4(0, 0, 0, 1);
    };

    std::vector<ModelData> g_models;

    std::unordered_map<std::string, FbxModel*> g_modelCache;
    std::unordered_map<FbxModel*, int>         g_refCount;

    int AllocHandle()
    {
        for (int i = 0; i < (int)g_models.size(); ++i)
        {
            if (!g_models[i].inUse) return i;
        }
        g_models.emplace_back();
        return (int)g_models.size() - 1;
    }

    bool IsValidHandle(int handle)
    {
        return handle >= 0 && handle < (int)g_models.size() && g_models[handle].inUse;
    }

    void ReleaseSharedModel(FbxModel* pFbx)
    {
        if (!pFbx) return;

        auto itRef = g_refCount.find(pFbx);
        if (itRef == g_refCount.end()) return;

        itRef->second--;
        if (itRef->second <= 0)
        {
            for (auto it = g_modelCache.begin(); it != g_modelCache.end(); ++it)
            {
                if (it->second == pFbx)
                {
                    g_modelCache.erase(it);
                    break;
                }
            }
            g_refCount.erase(itRef);
            delete pFbx;
        }
    }

    static int s_debug_draw_mesh_index = -1;

    void UpdateDebugSubMeshControl(FbxModel* pFbx)
    {
        if (!pFbx) return;

        const int meshCount = (int)pFbx->MeshGroup().MeshCount();

        if (Input::IsKeyDown(VK_F9))
        {
            s_debug_draw_mesh_index = (s_debug_draw_mesh_index < 0) ? 0 : -1;

            char buf[256];
            sprintf_s(buf, "[Step4] DebugDrawMeshIndex = %d (meshCount=%d)\n",
                s_debug_draw_mesh_index, meshCount);
            OutputDebugStringA(buf);
        }

        if (Input::IsKeyDown(VK_F10))
        {
            if (meshCount > 0)
            {
                if (s_debug_draw_mesh_index < 0) s_debug_draw_mesh_index = 0;
                else s_debug_draw_mesh_index = (s_debug_draw_mesh_index + 1) % meshCount;

                char buf[256];
                sprintf_s(buf, "[Step4] DebugDrawMeshIndex = %d (meshCount=%d)\n",
                    s_debug_draw_mesh_index, meshCount);
                OutputDebugStringA(buf);
            }
        }

        // SetDebugDrawMeshIndex is removed - feature not implemented in FbxModel
    }

    const ufbx_anim* ResolveAnim(const ufbx_scene* scene, const ModelData& md)
    {
        if (!scene) return nullptr;

        const ufbx_anim* anim = nullptr;

        if (md.anim.stackIndex >= 0 &&
            (size_t)md.anim.stackIndex < scene->anim_stacks.count)
        {
            const ufbx_anim_stack* stack = scene->anim_stacks.data[md.anim.stackIndex];
            if (stack) anim = stack->anim;
        }

        if (!anim && md.pFbx)
        {
            anim = md.pFbx->GetDefaultAnim();
        }

        return anim;
    }

    void UpdateAnimation(ModelData& md, const ufbx_anim* anim)
    {
        // FBX の fps を使う（未設定/異常値はフォールバック）
        double fps = 0.0;
        if (md.pFbx)
        {
            const ufbx_scene* scene = md.pFbx->Scene();
            if (scene) fps = scene->settings.frames_per_second;
        }
        if (fps <= 0.0) fps = 30.0;

        bool hasAnimSetting =
            (md.anim.endFrame > md.anim.startFrame) &&
            (md.anim.speed != 0.0f);

        if (anim && hasAnimSetting)
        {
            const float prevFrame = md.anim.currentFrame;

            if (!md.anim.paused)
            {
                const double dtSec = EngineTime::DeltaTime();
                const double deltaFrames = dtSec * fps * double(md.anim.speed);
                md.anim.currentFrame += (float)deltaFrames;

                if (md.anim.loop)
                {
                    float rangeLen = (float)(md.anim.endFrame - md.anim.startFrame + 1);
                    if (rangeLen <= 0.0f) rangeLen = 1.0f;

                    while (md.anim.currentFrame > md.anim.endFrame)   md.anim.currentFrame -= rangeLen;
                    while (md.anim.currentFrame < md.anim.startFrame) md.anim.currentFrame += rangeLen;
                }
                else
                {
                    if (md.anim.speed >= 0.0f)
                    {
                        if (md.anim.currentFrame > md.anim.endFrame)
                        {
                            md.anim.currentFrame = (float)md.anim.endFrame;
                            md.anim.paused = true;
                        }
                        if (md.anim.currentFrame < md.anim.startFrame)
                        {
                            md.anim.currentFrame = (float)md.anim.startFrame;
                        }
                    }
                    else
                    {
                        if (md.anim.currentFrame < md.anim.startFrame)
                        {
                            md.anim.currentFrame = (float)md.anim.startFrame;
                            md.anim.paused = true;
                        }
                        if (md.anim.currentFrame > md.anim.endFrame)
                        {
                            md.anim.currentFrame = (float)md.anim.endFrame;
                        }
                    }
                }
            }

            // paused / 同一フレームのときは timeSec を再計算せず、前回値を使う（double再計算ブレ回避）
            bool needRecalc = (md.anim.currentFrame != prevFrame);

            // 初回など timeSec が未設定の場合は再計算する
            if (!needRecalc && md.anim.timeSec == 0.0)
            {
                needRecalc = true;
            }

            if (needRecalc)
            {
                const double secondsPerFrame = 1.0 / fps;
                md.anim.timeSec = anim->time_begin + double(md.anim.currentFrame) * secondsPerFrame;
            }

            if (md.pFbx) md.pFbx->UpdateSkeletonAtTime(anim, md.anim.timeSec);
        }
        else if (anim)
        {
            md.anim.currentFrame = (float)md.anim.startFrame;

            const double secondsPerFrame = 1.0 / fps;
            md.anim.timeSec = anim->time_begin + double(md.anim.currentFrame) * secondsPerFrame;

            if (md.pFbx) md.pFbx->UpdateSkeletonAtTime(anim, md.anim.timeSec);
        }
        else
        {
            if (md.pFbx) md.pFbx->UpdateSkeletonAtTime(0.0);
        }
    }



    // Model-space root pose first, then world placement:
    // v' = (World * RootLocal) * v
    XMMATRIX BuildWorldMatrix(const ModelData& md)
    {
        // Transform の World（配置）は最後に効かせる。
        XMMATRIX world = XMMatrixIdentity();
        if (md.pTransform) world = md.pTransform->GetWorldMatrix();

        // モデル空間原点で確定するルート姿勢
        XMMATRIX rootS = XMMatrixScaling(md.rootScale, md.rootScale, md.rootScale);
        XMVECTOR q = XMLoadFloat4(&md.rootRotationQ);
        XMMATRIX rootR = XMMatrixRotationQuaternion(q);

        XMMATRIX rootLocal = rootS * rootR;

        // ★ここが重要：rootLocal を先に掛ける
        // これで「ルート姿勢はモデル空間」「Transformはワールド配置」になる
        return rootLocal * world;
    }
}

namespace Model
{
    void Initialize()
    {
        for (auto& md : g_models)
        {
            if (md.inUse && md.pFbx)
            {
                ReleaseSharedModel(md.pFbx);
                md.pFbx = nullptr;
                md.pTransform = nullptr;
                md.inUse = false;
            }
        }
        g_models.clear();

        g_modelCache.clear();
        g_refCount.clear();
    }

    int Load(std::string fileName)
    {
        int h = AllocHandle();
        auto& md = g_models[h];

        if (md.inUse && md.pFbx)
        {
            ReleaseSharedModel(md.pFbx);
            md.pFbx = nullptr;
        }

        FbxModel* pShared = nullptr;

        auto it = g_modelCache.find(fileName);
        if (it != g_modelCache.end())
        {
            pShared = it->second;
            g_refCount[pShared] += 1;
        }
        else
        {
            FbxModel* pNew = new FbxModel();
            if (!pNew->Load(fileName.c_str()))
            {
                delete pNew;
                return -1;
            }

            g_modelCache[fileName] = pNew;
            g_refCount[pNew] = 1;
            pShared = pNew;
        }

        md.pFbx = pShared;
        md.pTransform = nullptr;
        md.fileName = fileName;
        md.anim = AnimState{};
        md.rootScale = 1.0f;
        md.rootRotationQ = XMFLOAT4(0, 0, 0, 1);
        md.inUse = true;

        return h;
    }

    int Load(const std::string& fileName, float targetHeight)
    {
        int handle = Load(fileName);
        if (handle < 0) return handle;
        if (!IsValidHandle(handle)) return handle;

        ModelData& md = g_models[handle];
        if (!md.pFbx)
        {
            md.rootScale = 1.0f;
            return handle;
        }

        if (targetHeight <= 0.0f)
        {
            md.rootScale = 1.0f;
            return handle;
        }

        float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
        float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
        bool first = true;

        const ufbx_scene* scene = md.pFbx->Scene();
        if (scene && scene->root_node)
        {
            std::vector<const ufbx_node*> stack;
            stack.reserve(256);
            stack.push_back(scene->root_node);

            while (!stack.empty())
            {
                const ufbx_node* node = stack.back();
                stack.pop_back();
                if (!node) continue;

                const size_t cc = node->children.count;
                for (size_t i = 0; i < cc; ++i)
                {
                    const ufbx_node* c = node->children.data[i];
                    if (c) stack.push_back(c);
                }

                if (!node->mesh) continue;
                const ufbx_mesh* m = node->mesh;
                if (!m->vertex_position.exists) continue;

                const size_t vcount = m->vertex_position.values.count;
                if (vcount == 0) continue;

                const size_t MAX_SAMPLE = 20000;
                size_t step = 1;
                if (vcount > MAX_SAMPLE) step = vcount / MAX_SAMPLE;

                for (size_t vi = 0; vi < vcount; vi += step)
                {
                    const ufbx_vec3 p = m->vertex_position.values.data[vi];
                    const ufbx_vec3 wp = ufbx_transform_position(&node->geometry_to_world, p);

                    const float x = (float)wp.x;
                    const float y = (float)wp.y;
                    const float z = (float)wp.z;

                    if (first)
                    {
                        minX = maxX = x;
                        minY = maxY = y;
                        minZ = maxZ = z;
                        first = false;
                    }
                    else
                    {
                        if (x < minX) minX = x; if (x > maxX) maxX = x;
                        if (y < minY) minY = y; if (y > maxY) maxY = y;
                        if (z < minZ) minZ = z; if (z > maxZ) maxZ = z;
                    }
                }
            }
        }

        float sx = 0.0f, sy = 0.0f, sz = 0.0f;
        if (!first)
        {
            sx = (maxX - minX);
            sy = (maxY - minY);
            sz = (maxZ - minZ);
        }
        else
        {
            FbxMeshGroup& group = md.pFbx->MeshGroup();
            if (!group.Empty() && group.MeshCount() > 0)
            {
                const BVolume& bv = group.GetBV();
                sx = (bv.max.x - bv.min.x);
                sy = (bv.max.y - bv.min.y);
                sz = (bv.max.z - bv.min.z);
            }
            else
            {
                // SceneHeight is removed, use BVolume directly
                const BVolume& bv = md.pFbx->GetBV();
                sy = (bv.max.y - bv.min.y);
                sx = 0.0f;
                sz = 0.0f;
            }
        }

        float height = sy;

        if (scene)
        {
            switch (scene->settings.axes.up)
            {
            case UFBX_COORDINATE_AXIS_POSITIVE_X:
            case UFBX_COORDINATE_AXIS_NEGATIVE_X:
                height = sx;
                break;

            case UFBX_COORDINATE_AXIS_POSITIVE_Y:
            case UFBX_COORDINATE_AXIS_NEGATIVE_Y:
                height = sy;
                break;

            case UFBX_COORDINATE_AXIS_POSITIVE_Z:
            case UFBX_COORDINATE_AXIS_NEGATIVE_Z:
                height = sz;
                break;

            default:
                height = sx;
                if (sy > height) height = sy;
                if (sz > height) height = sz;
                break;
            }
        }

        // ===== root scale debug =====
        //DBG_LOG(
        //    "[RootScale] height=%.6f target=%.6f rootScale=%.6f finite(h=%d s=%d)\n",
        //    height,
        //    targetHeight,
        //    md.rootScale,
        //    std::isfinite(height),
        //    std::isfinite(md.rootScale)
        //);
        // ============================

        const float EPS = 1e-5f;
        if (height < EPS)
        {
            md.rootScale = 1.0f;
        }
        else
        {
            md.rootScale = targetHeight / height;
        }

        // ===== root scale debug =====
        //DBG_LOG(
        //    "[RootScale] height=%.6f target=%.6f rootScale=%.6f finite(h=%d s=%d)\n",
        //    height,
        //    targetHeight,
        //    md.rootScale,
        //    std::isfinite(height),
        //    std::isfinite(md.rootScale)
        //);
        // ============================

        return handle;
    }

    void Draw(int handle)
    {
        if (!IsValidHandle(handle)) return;

        auto& md = g_models[handle];
        if (!md.pFbx) return;

        const ufbx_scene* scene = md.pFbx->Scene();
        const ufbx_anim* anim = ResolveAnim(scene, md);

        UpdateAnimation(md, anim);

        const XMMATRIX world = BuildWorldMatrix(md);
        const XMMATRIX view = Camera::GetViewMatrix();
        const XMMATRIX proj = Camera::GetProjectionMatrix();

        md.pFbx->Draw(world, view, proj);
    }

    void DrawSkeleton(int handle)
    {
        if (!IsValidHandle(handle)) return;

        auto& md = g_models[handle];
        if (!md.pFbx) return;

        const XMMATRIX world = BuildWorldMatrix(md);
        const XMMATRIX view = Camera::GetViewMatrix();
        const XMMATRIX proj = Camera::GetProjectionMatrix();

        md.pFbx->DrawSkeleton(world, view, proj);
    }

    void Release(int handle)
    {
        if (!IsValidHandle(handle)) return;

        auto& md = g_models[handle];

        if (md.pFbx)
        {
            ReleaseSharedModel(md.pFbx);
            md.pFbx = nullptr;
        }

        md.pTransform = nullptr;
        md.inUse = false;
        md.fileName.clear();
    }

    void AllRelease()
    {
        for (auto& md : g_models)
        {
            if (md.inUse && md.pFbx)
            {
                ReleaseSharedModel(md.pFbx);
                md.pFbx = nullptr;
                md.inUse = false;
            }
            md.pTransform = nullptr;
            md.fileName.clear();
        }
        g_models.clear();

        g_modelCache.clear();
        g_refCount.clear();
    }

    void SetAnimFrame(int handle, int startFrame, int endFrame, float animSpeed)
    {
        if (!IsValidHandle(handle)) return;

        auto& md = g_models[handle];

        if (endFrame < startFrame) std::swap(startFrame, endFrame);

        md.anim.startFrame = startFrame;
        md.anim.endFrame = endFrame;
        md.anim.speed = animSpeed;
        md.anim.currentFrame = (float)startFrame;
        md.anim.timeSec = 0.0;
    }

    // 追加：現在選択中のアニメ（AnimStack/Default）をフル再生
    // ※フレーム指定再生は維持したまま、FBXの time_begin/time_end をフレームに変換して range を組む
    void SetAnimation(int handle, float animSpeed)
    {
        if (!IsValidHandle(handle)) return;

        auto& md = g_models[handle];
        const ufbx_scene* scene = md.pFbx ? md.pFbx->Scene() : nullptr;
        const ufbx_anim* anim = ResolveAnim(scene, md);
        if (!scene || !anim) return;

        double fps = scene->settings.frames_per_second;
        if (fps <= 0.0) fps = 30.0;

        const double duration = anim->time_end - anim->time_begin;
        if (duration <= 0.0) return;

        int endFrame = (int)(duration * fps + 0.5); // 四捨五入
        if (endFrame < 1) endFrame = 1;

        // 開始は 0 として扱う（UpdateAnimation 側で anim->time_begin を足す）
        md.anim.startFrame = 0;
        md.anim.endFrame = endFrame;
        md.anim.speed = animSpeed;
        md.anim.currentFrame = 0.0f;
        md.anim.timeSec = 0.0;

        // loop/paused は既存設定を尊重（必要なら呼び出し側で SetAnimLoop/SetAnimPaused）
    }

    int GetAnimFrame(int handle)
    {
        if (!IsValidHandle(handle)) return 0;
        return (int)g_models[handle].anim.currentFrame;
    }

    XMFLOAT3 GetBonePosition(int handle, std::string boneName)
    {
        if (!IsValidHandle(handle)) return XMFLOAT3(0, 0, 0);
        return XMFLOAT3(0, 0, 0);
    }

    XMFLOAT3 GetAnimBonePosition(int handle, std::string boneName)
    {
        if (!IsValidHandle(handle)) return XMFLOAT3(0, 0, 0);
        return XMFLOAT3(0, 0, 0);
    }

    void SetTransform(int handle, Transform& t)
    {
        if (!IsValidHandle(handle)) return;
        g_models[handle].pTransform = &t;
    }

    XMMATRIX GetMatrix(int handle)
    {
        if (!IsValidHandle(handle)) return XMMatrixIdentity();
        return BuildWorldMatrix(g_models[handle]);
    }

    void RayCast(int handle, RayCastData* data)
    {
        if (!IsValidHandle(handle) || !data) return;
    }

    int GetAnimStackCount(int handle)
    {
        if (!IsValidHandle(handle)) return 0;

        auto& md = g_models[handle];
        const ufbx_scene* scene = md.pFbx ? md.pFbx->Scene() : nullptr;
        if (!scene) return 0;

        return (int)scene->anim_stacks.count;
    }

    std::string GetAnimStackName(int handle, int index)
    {
        if (!IsValidHandle(handle)) return {};

        auto& md = g_models[handle];
        const ufbx_scene* scene = md.pFbx ? md.pFbx->Scene() : nullptr;
        if (!scene) return {};

        if (index < 0 || (size_t)index >= scene->anim_stacks.count) return {};

        const ufbx_anim_stack* stack = scene->anim_stacks.data[index];
        if (!stack) return {};

        return std::string(stack->name.data, stack->name.length);
    }

    void SetAnimStack(int handle, int index)
    {
        if (!IsValidHandle(handle)) return;

        auto& md = g_models[handle];
        const ufbx_scene* scene = md.pFbx ? md.pFbx->Scene() : nullptr;
        if (!scene) return;

        if (index < 0 || (size_t)index >= scene->anim_stacks.count) return;

        md.anim.stackIndex = index;
        md.anim.currentFrame = (float)md.anim.startFrame;
    }

    void SetAnimStack(int handle, const std::string& stackName)
    {
        if (!IsValidHandle(handle)) return;

        auto& md = g_models[handle];
        const ufbx_scene* scene = md.pFbx ? md.pFbx->Scene() : nullptr;
        if (!scene) return;

        for (size_t i = 0; i < scene->anim_stacks.count; ++i)
        {
            const ufbx_anim_stack* stack = scene->anim_stacks.data[i];
            if (!stack) continue;

            if (stackName.size() == stack->name.length &&
                std::memcmp(stackName.c_str(), stack->name.data, stack->name.length) == 0)
            {
                md.anim.stackIndex = (int)i;
                md.anim.currentFrame = (float)md.anim.startFrame;
                return;
            }
        }
    }

    void SetAnimPaused(int handle, bool paused)
    {
        if (!IsValidHandle(handle)) return;
        g_models[handle].anim.paused = paused;
    }

    bool IsAnimPaused(int handle)
    {
        if (!IsValidHandle(handle)) return false;
        return g_models[handle].anim.paused;
    }

    void SetAnimLoop(int handle, bool loop)
    {
        if (!IsValidHandle(handle)) return;
        g_models[handle].anim.loop = loop;
    }

    bool IsAnimLoop(int handle)
    {
        if (!IsValidHandle(handle)) return true;
        return g_models[handle].anim.loop;
    }

    void SetRootScale(int handle, float rootScale)
    {
        if (!IsValidHandle(handle)) return;
        g_models[handle].rootScale = rootScale;
    }

    float GetRootScale(int handle)
    {
        if (!IsValidHandle(handle)) return 1.0f;
        return g_models[handle].rootScale;
    }

    void SetRootRotationYawPitchRoll(int handle, float yaw, float pitch, float roll)
    {
        if (!IsValidHandle(handle)) return;

        XMVECTOR q = XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
        XMStoreFloat4(&g_models[handle].rootRotationQ, q);
    }

    void SetRootRotationQuaternion(int handle, const DirectX::XMFLOAT4& q)
    {
        if (!IsValidHandle(handle)) return;
        g_models[handle].rootRotationQ = q;
    }

    DirectX::XMFLOAT4 GetRootRotationQuaternion(int handle)
    {
        if (!IsValidHandle(handle)) return DirectX::XMFLOAT4(0, 0, 0, 1);
        return g_models[handle].rootRotationQ;
    }

    void SetRootRotationYawPitchRollDeg(int handle, float yawDeg, float pitchDeg, float rollDeg)
    {
        if (!IsValidHandle(handle)) return;

        const float DEG2RAD = DirectX::XM_PI / 180.0f;

        const float yaw = yawDeg * DEG2RAD;
        const float pitch = pitchDeg * DEG2RAD;
        const float roll = rollDeg * DEG2RAD;

        XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
        DirectX::XMStoreFloat4(&g_models[handle].rootRotationQ, q);
    }
}
