
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

		// これを「ルートスケール」として扱う（world = S * worldTransform）
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
			world = s * world; // ルートスケールを先に掛ける（平行移動もスケールされる）
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

		// 既に使っていたら解放
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

	// 既存：targetHeight でスケール正規化してロード
	int Load(const std::string& fileName, float targetHeight)
	{
		int handle = Load(fileName);
		if (handle < 0) return handle;
		if (!IsValidHandle(handle)) return handle;

		ModelData& md = g_models[handle];
		if (!md.pFbx)
		{
			md.uniformScale = 1.0f;
			return handle;
		}

		if (targetHeight <= 0.0f)
		{
			md.uniformScale = 1.0f;
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
				sy = md.pFbx->SceneHeight();
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

		const float EPS = 1e-5f;
		if (height < EPS)
		{
			md.uniformScale = 1.0f;
		}
		else
		{
			md.uniformScale = targetHeight / height;
		}

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

	// ------------------------------------------------------------
	// 追加：ルートスケール（手動正規化）
	// ※ targetHeight 正規化（Load(file, targetHeight)）を壊さないため、
	//    ModelComponent 側は「明示的に上書きしたときだけ」呼ぶ運用にする。
	// ------------------------------------------------------------
	void SetRootScale(int handle, float rootScale)
	{
		if (!IsValidHandle(handle)) return;
		g_models[handle].uniformScale = rootScale;
	}

	float GetRootScale(int handle)
	{
		if (!IsValidHandle(handle)) return 1.0f;
		return g_models[handle].uniformScale;
	}
}
