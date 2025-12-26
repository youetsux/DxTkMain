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
#include <cstring>   // memcmp
#include <utility>   // swap

using namespace DirectX;

//====================================
// 内部管理用構造体 & キャッシュ
//====================================
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
        FbxModel* pFbx = nullptr;        // 共有リソース（所有権なし）
        Transform* pTransform = nullptr;  // 外部の Transform（所有権なし）

        std::string  fileName;

        AnimState    anim;
        bool         inUse = false;

        float        uniformScale = 1.0f;
    };

    std::vector<ModelData> g_models;

    std::unordered_map<std::string, FbxModel*> g_modelCache; // file -> shared
    std::unordered_map<FbxModel*, int>         g_refCount;   // shared -> refcount

    // ------------------------------------------------------------
    // Handle utils
    // ------------------------------------------------------------
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

    // ------------------------------------------------------------
    // Shared model release
    // ------------------------------------------------------------
    void ReleaseSharedModel(FbxModel* pFbx)
    {
        if (!pFbx) return;

        auto itRef = g_refCount.find(pFbx);
        if (itRef == g_refCount.end()) return;

        itRef->second--;
        if (itRef->second <= 0)
        {
            // キャッシュからも消す（同一ポインタを探す）
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

    // ------------------------------------------------------------
    // Step4: sub-mesh solo draw control
    //   F9  : toggle solo draw (all <-> 0)
    //   F10 : next sub-mesh (when solo draw enabled)
    // ------------------------------------------------------------
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

    // ------------------------------------------------------------
    // Animation update
    // ------------------------------------------------------------
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
        constexpr double ANIM_FPS = 60.0;

        bool hasAnimSetting =
            (md.anim.endFrame > md.anim.startFrame) &&
            (md.anim.speed != 0.0f);

        if (anim && hasAnimSetting)
        {
            if (!md.anim.paused)
            {
                const double dtSec = EngineTime::DeltaTime();
                const double deltaFrames = dtSec * ANIM_FPS * double(md.anim.speed);
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

            const double secondsPerFrame = 1.0 / ANIM_FPS;
            double tSec = anim->time_begin + double(md.anim.currentFrame) * secondsPerFrame;

            if (md.pFbx) md.pFbx->UpdateSkeletonAtTime(anim, tSec);
        }
        else if (anim)
        {
            md.anim.currentFrame = (float)md.anim.startFrame;

            const double secondsPerFrame = 1.0 / ANIM_FPS;
            double tSec = anim->time_begin + double(md.anim.currentFrame) * secondsPerFrame;

            if (md.pFbx) md.pFbx->UpdateSkeletonAtTime(anim, tSec);
        }
        else
        {
            if (md.pFbx) md.pFbx->UpdateSkeletonAtTime(0.0);
        }
    }

    // ------------------------------------------------------------
    // Matrix
    // ------------------------------------------------------------
    XMMATRIX BuildWorldMatrix(const ModelData& md)
    {
        XMMATRIX world = XMMatrixIdentity();
        if (md.pTransform) world = md.pTransform->GetWorldMatrix();

        if (md.uniformScale != 1.0f)
        {
            XMMATRIX s = XMMatrixScaling(md.uniformScale, md.uniformScale, md.uniformScale);
            world = s * world;
        }
        return world;
    }
}

//====================================
// Model 名前空間 実装
//====================================
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
        md.uniformScale = 1.0f;
        md.inUse = true;

        return h;
    }

    int Load(const std::string& fileName, float targetHeight)
    {
        // まずは通常版 Load でモデルを読み込む
        int handle = Load(fileName);
        if (handle < 0) return handle;
        if (!IsValidHandle(handle)) return handle;

        ModelData& md = g_models[handle];
        if (!md.pFbx)
        {
            md.uniformScale = 1.0f;
            return handle;
        }

        // targetHeight <= 0 の場合は正規化しない
        if (targetHeight <= 0.0f)
        {
            md.uniformScale = 1.0f;
            return handle;
        }

        float srcHeight = 0.0f;
        bool  hasHeight = false;

        // ------------------------------------------------------------
        // 1) まずは「メッシュローカル高さ」を従来どおり求める
        //    - 複数メッシュ: MeshGroup 全体の skinned_vertices_ から取得
        //    - それで取れなければ MeasureSkinnedHeightY() にフォールバック
        // ------------------------------------------------------------
        FbxMeshGroup& group = md.pFbx->MeshGroup();
        if (!group.Empty() && group.MeshCount() > 0)
        {
            const auto& meshes = group.Meshes();

            bool  first = true;
            float minY = 0.0f;
            float maxY = 0.0f;

            for (size_t mi = 0; mi < meshes.size(); ++mi)
            {
                const std::unique_ptr<FbxMesh>& mesh = meshes[mi];
                if (!mesh) continue;

                const auto& data = mesh->Data();

                // スキニング後頂点が存在するときだけ使う
                if (!data.skinned_vertices_.empty() &&
                    !data.influences_.empty() &&
                    !data.bind_vertices_.empty())
                {
                    for (size_t vi = 0; vi < data.skinned_vertices_.size(); ++vi)
                    {
                        const auto& v = data.skinned_vertices_[vi];

                        if (first)
                        {
                            minY = maxY = v.pos.y;
                            first = false;
                        }
                        else
                        {
                            if (v.pos.y < minY) minY = v.pos.y;
                            if (v.pos.y > maxY) maxY = v.pos.y;
                        }
                    }
                }
            }

            if (!first)
            {
                srcHeight = maxY - minY;
                hasHeight = true;
            }
        }

        // グループから有効な高さが取れなかった場合：
        // シングルメッシュ用の従来ロジックにフォールバック
        const float EPS = 1e-5f;
        if (!hasHeight)
        {
            srcHeight = md.pFbx->MeasureSkinnedHeightY();
        }

        // ------------------------------------------------------------
        // 2) root 直下の「メッシュを持つノード」の local_scale を拾う
        //    - SillyDancing: だいたい 1.0
        //    - TriAvater   : だいたい 100.0
        // ------------------------------------------------------------
        float nodeScale = 1.0f;
        bool  hasNodeScale = false;

        const ufbx_scene* scene = md.pFbx->Scene();
        if (scene && scene->root_node)
        {
            const ufbx_node* root = scene->root_node;
            size_t childCount = root->children.count;

            for (size_t i = 0; i < childCount; ++i)
            {
                const ufbx_node* child = root->children.data[i];
                if (!child) continue;

                // メッシュを持つノードだけを見る（Eye, Body.001 など）
                if (!child->mesh) continue;

                const ufbx_transform& lt = child->local_transform;

                double sx = lt.scale.x;
                double sy = lt.scale.y;
                double sz = lt.scale.z;

                // 一応 3軸の平均値をスケールとみなす
                double sum = sx + sy + sz;
                double s = sum / 3.0;

                if (s > 0.0)
                {
                    float sf = (float)s;

                    if (!hasNodeScale)
                    {
                        nodeScale = sf;
                        hasNodeScale = true;
                    }
                    else
                    {
                        // 複数メッシュがあっても極端にブレないように簡単に平均
                        nodeScale = (nodeScale + sf) * 0.5f;
                    }
                }
            }
        }

        // ------------------------------------------------------------
        // 3) 「実効高さ = メッシュローカル高さ × ノードスケール」で正規化スケール決定
        //
        //    worldHeight ≒ srcHeight * nodeScale * uniformScale
        //    → uniformScale = targetHeight / (srcHeight * nodeScale)
        //
        //    SillyDancing : nodeScale ≒ 1   → 従来どおり
        //    TriAvater   : nodeScale ≒ 100 → 100倍された分をここで打ち消す
        // ------------------------------------------------------------
        float effectiveHeight = srcHeight;

        if (hasNodeScale)
        {
            effectiveHeight *= nodeScale;
        }

        if (effectiveHeight < EPS)
        {
            md.uniformScale = 1.0f;
        }
        else
        {
            md.uniformScale = targetHeight / effectiveHeight;
        }

        return handle;
    }

    void Draw(int handle)
    {
        if (!IsValidHandle(handle)) return;

        auto& md = g_models[handle];
        if (!md.pFbx) return;

        // Step4: サブメッシュ単体描画デバッグ（必要時のみ使用）
        UpdateDebugSubMeshControl(md.pFbx);

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

        auto& md = g_models[handle];
        if (md.pTransform) return md.pTransform->GetWorldMatrix();
        return XMMatrixIdentity();
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
}
