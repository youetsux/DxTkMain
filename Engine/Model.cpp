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

        pFbx->SetDebugDrawMeshIndex(s_debug_draw_mesh_index);
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
            // scene を保持しない運用（ベイク済み）の場合もここで取得する
            fps = md.pFbx->GetRuntimeAnimFps(md.anim.stackIndex);
        }
        if (fps <= 0.0) fps = 30.0;

        bool hasAnimSetting =
            (md.anim.endFrame > md.anim.startFrame) &&
            (md.anim.speed != 0.0f);

        if (md.pFbx && hasAnimSetting)
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
                md.anim.timeSec = 0.0; // scene 破棄運用では使用しない
            }

            if (md.pFbx) md.pFbx->UpdateSkeletonAtFrame(md.anim.stackIndex, md.anim.currentFrame);
        }
        else if (md.pFbx)
        {
            md.anim.currentFrame = (float)md.anim.startFrame;

            const double secondsPerFrame = 1.0 / fps;
            md.anim.timeSec = 0.0; // scene 破棄運用では使用しない

            if (md.pFbx) md.pFbx->UpdateSkeletonAtFrame(md.anim.stackIndex, md.anim.currentFrame);
        }
        else
        {
            if (md.pFbx) md.pFbx->UpdateSkeletonAtFrame(md.anim.stackIndex, 0.0);
        }
    }

    namespace
    {
        inline void DBG_LOG(const char* fmt, ...)
        {
#ifdef _DEBUG
            char buf[1024];

            va_list args;
            va_start(args, fmt);
            vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, args);
            va_end(args);

            // Visual Studio の「出力」ウィンドウ
            OutputDebugStringA(buf);

            // コンソール（必要なら）
            std::printf("%s", buf);
#endif
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

        // 足元合わせ: MeasuredMinUp() を 0 に持ち上げる
        // (rootS が先に掛かるので、平行移動量も rootScale を掛けた値にする)
        float groundUp = 0.0f;
        if (md.pFbx)
        {
            groundUp = md.pFbx->MeasuredMinUp();
        }
        XMMATRIX rootT = XMMatrixTranslation(0.0f, -groundUp * md.rootScale, 0.0f);

        XMMATRIX rootLocal = rootS * rootR * rootT;

        // ★ここが重要：rootLocal を先に掛ける
        // これで「ルート姿勢はモデル空間」「Transformはワールド配置」になる
        XMFLOAT4X4 m;
        XMStoreFloat4x4(&m, rootLocal * world);
        DBG_LOG("[World] scale=%.6f pos=(%.3f,%.3f,%.3f)\n",
            md.rootScale, m._41, m._42, m._43);

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


    namespace
    {
        inline void DBG_LOG(const char* fmt, ...)
        {
#ifdef _DEBUG
            char buf[1024];

            va_list args;
            va_start(args, fmt);
            vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, args);
            va_end(args);

            // Visual Studio の「出力」ウィンドウ
            OutputDebugStringA(buf);

            // コンソール（必要なら）
            std::printf("%s", buf);
#endif
        }
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

        // scene を保持しない運用でも targetHeight 正規化が安定するように、
        // FbxModel 側でロード時に計測した実寸（geometry_to_world 適用済み・unit_meters 適用済み）を優先する。
        DBG_LOG("[Measured] h=%.6f max=%.6f\n",
            md.pFbx->MeasuredHeight(),
            md.pFbx->MeasuredMaxExtent());

        float height = md.pFbx->MeasuredHeight();
        if (height <= 1e-5f)
        {
            height = md.pFbx->MeasuredMaxExtent();
        }

        // それでも取れない場合のみ、既存のフォールバックへ
        if (height <= 1e-5f)
        {
            FbxMeshGroup& group = md.pFbx->MeshGroup();
            if (!group.Empty() && group.MeshCount() > 0)
            {
                const BVolume& bv = group.GetBV();
                const float sx = (bv.max.x - bv.min.x);
                const float sy = (bv.max.y - bv.min.y);
                const float sz = (bv.max.z - bv.min.z);

                // scene が無い場合、代表サイズとして最大軸長を採用（寝ている等でsyが薄いケース対策）
                height = sx;
                if (sy > height) height = sy;
                if (sz > height) height = sz;
            }
            else
            {
                // 最終フォールバック（SceneHeight は実装依存だがゼロよりはマシ）
                height = md.pFbx->SceneHeight();
            }
        }

        const float EPS = 1e-5f;
        if (height < EPS)
        {
            md.rootScale = 1.0f;
        }
        else
        {
            md.rootScale = targetHeight / height;
        }

        //DBG_LOG("[RootScale] target=%.6f height=%.6f rootScale=%.9f\n",
        //    targetHeight, height, md.rootScale);

        return handle;
    }







    void Draw(int handle)
    {
        if (!IsValidHandle(handle)) return;

        auto& md = g_models[handle];
        if (!md.pFbx) return;

        UpdateDebugSubMeshControl(md.pFbx);

        const ufbx_scene* scene = md.pFbx->Scene();
        const ufbx_anim* anim = ResolveAnim(scene, md);

        UpdateAnimation(md, anim);

        const XMMATRIX world = BuildWorldMatrix(md);   // ← ここで rootScale/rootRotation 済み
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
        if (!md.pFbx) return;

        // scene を保持しない運用（ベイク済み）でも成立するように、runtime 情報から range を組む
        const int endFrame = md.pFbx->GetRuntimeAnimEndFrame(md.anim.stackIndex);
        md.anim.startFrame = 0;
        md.anim.endFrame = endFrame;
        md.anim.speed = animSpeed;
        md.anim.currentFrame = 0.0f;
        md.anim.timeSec = 0.0;
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
        if (!md.pFbx) return 0;
        return md.pFbx->GetRuntimeAnimStackCount();
    }


    std::string GetAnimStackName(int handle, int index)
    {
        if (!IsValidHandle(handle)) return {};
        auto& md = g_models[handle];
        if (!md.pFbx) return {};
        return md.pFbx->GetRuntimeAnimStackName(index);
    }


    void SetAnimStack(int handle, int index)
    {
        if (!IsValidHandle(handle)) return;

        auto& md = g_models[handle];
        if (!md.pFbx) return;

        const int count = md.pFbx->GetRuntimeAnimStackCount();
        if (count <= 0) return;
        if (index < 0 || index >= count) return;

        md.anim.stackIndex = index;
        md.anim.currentFrame = (float)md.anim.startFrame;
    }


    void SetAnimStack(int handle, const std::string& stackName)
    {
        if (!IsValidHandle(handle)) return;

        auto& md = g_models[handle];
        if (!md.pFbx) return;

        const int count = md.pFbx->GetRuntimeAnimStackCount();
        for (int i = 0; i < count; ++i)
        {
            if (md.pFbx->GetRuntimeAnimStackName(i) == stackName)
            {
                md.anim.stackIndex = i;
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
