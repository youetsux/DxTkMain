#include "Model.h"
#include "FbxModel.h"
#include "Camera.h"

#include <vector>
#include <string>
#include "EngineTime.h"  // ★追加
#include <unordered_map>

using namespace DirectX;

//====================================
// 内部管理用構造体 & キャッシュ
//====================================
namespace
{
    // アニメーション関連の状態をひとまとめにする
    struct AnimState
    {
        int    startFrame = 0;
        int    endFrame = 0;
        float  speed = 0.0f;   // 倍速
        float  currentFrame = 0.0f;   // 表示用の現在フレーム

        int    stackIndex = -1;
        double timeSec = 0.0;    // 内部用のアニメ時間（秒）

        bool   paused = false;  // ★ ポーズ中なら true
        bool   loop = true;   // ★ 将来用：ループ再生するか
    };
    // ハンドルごとの「インスタンス」データ
    struct ModelData
    {
        FbxModel* pFbx = nullptr;   // 共有リソース（所有権なし）
        Transform* pTransform = nullptr;   // 外部の Transform（所有権なし）

        std::string fileName;              // 読み込んだファイルパス


        AnimState anim;          // ★ ここに集約
        bool inUse = false;

        // ★ 追加：モデルごとの一括スケール（デフォルト 1.0 = そのまま）
        float  uniformScale = 1.0f;
    };

    std::vector<ModelData> g_models;

    // ファイルパス → 共有 FbxModel*
    std::unordered_map<std::string, FbxModel*> g_modelCache;

    // 共有 FbxModel* → 参照カウント
    std::unordered_map<FbxModel*, int> g_refCount;

    int AllocHandle()
    {
        for (int i = 0; i < static_cast<int>(g_models.size()); ++i)
        {
            if (!g_models[i].inUse)
                return i;
        }
        g_models.emplace_back();
        return static_cast<int>(g_models.size() - 1);
    }

    bool IsValidHandle(int handle)
    {
        //return handle >= 0
        //    && handle < static_cast<int>(g_models.size())
        //    && g_models[handle].inUse
        //    && g_models[handle].pFbx != nullptr;
        return handle >= 0 && handle < (int)g_models.size() && g_models[handle].inUse;

    }

    // 共有モデルの参照カウントを減らし、0 になったら削除
    void ReleaseSharedModel(FbxModel* pFbx, const std::string& fileName)
    {
        if (!pFbx) return;

        auto itRef = g_refCount.find(pFbx);
        if (itRef == g_refCount.end()) return;

        itRef->second--;
        if (itRef->second <= 0)
        {
            // キャッシュからも消す
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

}

//====================================
// Model 名前空間 実装
//====================================
namespace Model
{
    void Initialize()
    {
        // すべてのハンドルから参照を外しつつ、
        // 共有モデルの参照カウントを減らす
        for (auto& md : g_models)
        {
            if (md.inUse && md.pFbx)
            {
                ReleaseSharedModel(md.pFbx, md.fileName);
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

        // すでにこのハンドルが別モデルを指していた場合は解放
        if (md.inUse && md.pFbx)
        {
            ReleaseSharedModel(md.pFbx, md.fileName);
            md.pFbx = nullptr;
        }

        // 共有キャッシュに同じファイルがあるか？
        FbxModel* pShared = nullptr;

        auto it = g_modelCache.find(fileName);
        if (it != g_modelCache.end())
        {
            // 既にロード済み → 共有
            pShared = it->second;
            g_refCount[pShared] += 1;
        }
        else
        {
            // 初回ロード
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
        // まずは通常版 Load で読み込む（共有キャッシュを利用）
        int handle = Load(fileName);
        if (handle < 0) {
            return handle;
        }
        if (!IsValidHandle(handle)) {
            return handle;
        }

        auto& md = g_models[handle];
        if (!md.pFbx) {
            // 異常ケース：とりあえずスケール 1.0 のまま
            md.uniformScale = 1.0f;
            return handle;
        }

        // FbxModel から「高さ（Y サイズ）」を取得
        //float srcHeight = md.pFbx->MeasureSize(SizeMeasureAxis::HeightY);
        float srcHeight = md.pFbx->MeasureSkinnedHeightY();
        if (fileName.find("TriAvater") != std::string::npos) {
            OutputDebugStringA(
                (std::string("TriAvater: srcHeight = ")
                    + std::to_string(srcHeight) + "\n").c_str());
        }




        const float EPS = 1e-5f;
        if (srcHeight < EPS || targetHeight <= 0.0f)
        {
            // 異常ケース：スケール 1.0 のまま
            md.uniformScale = 1.0f;
        }
        else
        {
            // 高さが targetHeight になるように一括スケール
            md.uniformScale = targetHeight / srcHeight;
        }

        // ★ ここを一時的に入れて、TriAvatar の実際の値を確認する
        //if (fileName.find("TriAvater") != std::string::npos) {
        //    float worldHeight = srcHeight * md.uniformScale;  // Transform の scale が 1 前提

        //    char buf[256];
        //    sprintf_s(buf,
        //        "TriAvater: srcHeight = %.3f, targetHeight = %.3f, uniformScale = %.6f, worldHeight = %.3f\n",
        //        srcHeight, targetHeight, md.uniformScale, worldHeight);
        //    OutputDebugStringA(buf);
        //}


        return handle;
    }
    

    void Draw(int handle)
    {
        constexpr double ANIM_FPS = 60.0;
        if (!IsValidHandle(handle)) return;

        auto& md = g_models[handle];
        if (!md.pFbx) return;

        // ★ デバッグログ追加
 /*       {
            char buf[256];
            sprintf_s(buf,
                "[Draw] h=%d  uniformScale=%.6f  HeightY=%.6f\n",
                handle,
                md.uniformScale,
                md.pFbx->MeasureSize(SizeMeasureAxis::HeightY));
            OutputDebugStringA(buf);
        }*/

        const ufbx_scene* scene = md.pFbx ? md.pFbx->Scene() : nullptr;

        // 1) 使うアニメを決める（AnimStack 指定があれば優先）
        const ufbx_anim* anim = nullptr;
        if (scene) {
            if (md.anim.stackIndex >= 0 &&
                (size_t)md.anim.stackIndex < scene->anim_stacks.count)
            {
                const ufbx_anim_stack* stack = scene->anim_stacks.data[md.anim.stackIndex];
                if (stack) {
                    anim = stack->anim;  // AnimStack に対応する ufbx_anim
                }
            }

            // AnimStack 未指定 or 無効 → デフォルトアニメ
            if (!anim) {
                anim = md.pFbx->GetDefaultAnim();
            }
        }

        bool hasAnimSetting =
            (md.anim.endFrame > md.anim.startFrame) && (md.anim.speed != 0.0f);

        if (anim && hasAnimSetting)
        {
            if (!md.anim.paused){
                // ★ここだけ「時間ベース」に変える
                const double dtSec = EngineTime::DeltaTime();      // 秒
                const double framesPerSec = ANIM_FPS;                // 60fps 基準
                const double deltaFrames = dtSec * framesPerSec * double(md.anim.speed);

                // フレーム番号を時間に応じて増やす
                md.anim.currentFrame += static_cast<float>(deltaFrames);

                if (md.anim.loop) {
                    // 範囲 [startFrame, endFrame] 内でループ
                    float rangeLen = float(md.anim.endFrame - md.anim.startFrame + 1);
                    if (rangeLen <= 0.0f) rangeLen = 1.0f;

                    while (md.anim.currentFrame > md.anim.endFrame)   md.anim.currentFrame -= rangeLen;
                    while (md.anim.currentFrame < md.anim.startFrame) md.anim.currentFrame += rangeLen;
                }
                else
                {
                    // 一回再生：終端で止める
                    if (md.anim.speed >= 0.0f)
                    {
                        if (md.anim.currentFrame > md.anim.endFrame)
                        {
                            md.anim.currentFrame = static_cast<float>(md.anim.endFrame);
                            md.anim.paused = true; // 自動ポーズ
                        }
                        if (md.anim.currentFrame < md.anim.startFrame)
                        {
                            md.anim.currentFrame = static_cast<float>(md.anim.startFrame);
                        }
                    }
                    else
                    {
                        // 逆再生も一応ケアしておくならこうする
                        if (md.anim.currentFrame < md.anim.startFrame)
                        {
                            md.anim.currentFrame = static_cast<float>(md.anim.startFrame);
                            md.anim.paused = true;
                        }
                        if (md.anim.currentFrame > md.anim.endFrame)
                        {
                            md.anim.currentFrame = static_cast<float>(md.anim.endFrame);
                        }
                    }
                }
            }

            // ここから先の「frame → time 変換」は旧ロジックと同じ
            const double secondsPerFrame = 1.0 / ANIM_FPS;
            double tSec = anim->time_begin
                + double(md.anim.currentFrame) * secondsPerFrame;

            md.pFbx->UpdateSkeletonAtTime(anim, tSec);
        }
        else if (anim)
        {
            // アニメはあるが SetAnimFrame されていない → 先頭フレームで固定
            md.anim.currentFrame = static_cast<float>(md.anim.startFrame);
            const double secondsPerFrame = 1.0 / ANIM_FPS;
            double tSec = anim->time_begin
                + double(md.anim.currentFrame) * secondsPerFrame;
            md.pFbx->UpdateSkeletonAtTime(anim, tSec);
        }
        else
        {
            // アニメ自体がない場合
            md.pFbx->UpdateSkeletonAtTime(0.0);
        }

        // --- ここから下は既存の描画処理と同じ ---

        DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();
        if (md.pTransform) {
            world = md.pTransform->GetWorldMatrix();
        }
        // ★ モデルごとの正規化スケールを反映
        if (md.uniformScale != 1.0f)
        {
            XMMATRIX s = XMMatrixScaling(md.uniformScale,
                md.uniformScale,
                md.uniformScale);
            world = s * world;
        }


        DirectX::XMMATRIX view = Camera::GetViewMatrix();
        DirectX::XMMATRIX proj = Camera::GetProjectionMatrix();

        md.pFbx->Draw(world, view, proj);
    }


    void DrawSkeleton(int handle)
    {
        if (!IsValidHandle(handle)) return;

        auto& md = g_models[handle];
        if (!md.pFbx) return;

        using namespace DirectX;

        // ワールド行列
        XMMATRIX world = XMMatrixIdentity();
        if (md.pTransform)
        {
            world = md.pTransform->GetWorldMatrix();
        }
        // ★ モデル本体と同じ正規化スケールを反映
        if (md.uniformScale != 1.0f)
        {
            XMMATRIX s = XMMatrixScaling(md.uniformScale,
                md.uniformScale,
                md.uniformScale);
            world = s * world;
        }



        // カメラからビュー・プロジェクションを取得
        XMMATRIX view = Camera::GetViewMatrix();
        XMMATRIX proj = Camera::GetProjectionMatrix();

        // FbxModel 側のスケルトン描画
        md.pFbx->DrawSkeleton(world, view, proj);
    }




    void Release(int handle)
    {
        if (!IsValidHandle(handle)) return;

        auto& md = g_models[handle];

        if (md.pFbx)
        {
            ReleaseSharedModel(md.pFbx, md.fileName);
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
                ReleaseSharedModel(md.pFbx, md.fileName);
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

        if (endFrame < startFrame)
        {
            std::swap(startFrame, endFrame);
        }

        md.anim.startFrame = startFrame;
        md.anim.endFrame = endFrame;
        md.anim.speed = animSpeed;
        md.anim.currentFrame = float(startFrame);
        md.anim.timeSec = 0.0;        // ★内部時間もリセット
    }

    int GetAnimFrame(int handle)
    {
        if (!IsValidHandle(handle)) return 0;
        return static_cast<int>(g_models[handle].anim.currentFrame);
    }

    XMFLOAT3 GetBonePosition(int handle, std::string boneName)
    {
        if (!IsValidHandle(handle)) return XMFLOAT3(0, 0, 0);
        // TODO: 後で実装
        return XMFLOAT3(0, 0, 0);
    }

    XMFLOAT3 GetAnimBonePosition(int handle, std::string boneName)
    {
        if (!IsValidHandle(handle)) return XMFLOAT3(0, 0, 0);
        // TODO: 後で実装
        return XMFLOAT3(0, 0, 0);
    }

    void SetTransform(int handle, Transform& t)
    {
        if (!IsValidHandle(handle)) return;

        g_models[handle].pTransform = &t;
    }

    XMMATRIX GetMatrix(int handle)
    {
        if (!IsValidHandle(handle))
            return XMMatrixIdentity();

        auto& md = g_models[handle];
        if (md.pTransform)
        {
            return md.pTransform->GetWorldMatrix();
        }
        return XMMatrixIdentity();
    }

    void RayCast(int handle, RayCastData* data)
    {
        if (!IsValidHandle(handle) || !data) return;
        // TODO: 後で FbxMesh 連携で実装
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

        // ufbx_string を std::string に変換
        return std::string(stack->name.data, stack->name.length);
    }

    void SetAnimStack(int handle, int index)
    {
        if (!IsValidHandle(handle)) return;

        auto& md = g_models[handle];
        const ufbx_scene* scene = md.pFbx ? md.pFbx->Scene() : nullptr;
        if (!scene) return;

        if (index < 0 || (size_t)index >= scene->anim_stacks.count) {
            // 不正な値は無視（デフォルト anim のまま）
            return;
        }

        md.anim.stackIndex = index;

        // 新しい AnimStack に切り替えたので、フレームを先頭に戻しておく
        md.anim.currentFrame = (float)md.anim.startFrame;
    }

    void SetAnimStack(int handle, const std::string& stackName)
    {
        if (!IsValidHandle(handle)) return;

        auto& md = g_models[handle];
        const ufbx_scene* scene = md.pFbx ? md.pFbx->Scene() : nullptr;
        if (!scene) return;

        for (size_t i = 0; i < scene->anim_stacks.count; ++i) {
            const ufbx_anim_stack* stack = scene->anim_stacks.data[i];
            if (!stack) continue;

            // ufbx_string と std::string を長さつき比較
            if (stackName.size() == stack->name.length &&
                std::memcmp(stackName.c_str(), stack->name.data, stack->name.length) == 0)
            {
                md.anim.stackIndex = (int)i;
                md.anim.currentFrame = (float)md.anim.startFrame;
                return;
            }
        }

        // 見つからなかったら何もしない（デフォルト anim のまま）
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
        if (!IsValidHandle(handle)) return true; // デフォルトはループ扱い
        return g_models[handle].anim.loop;
    }

}
