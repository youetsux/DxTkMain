#include "FbxSkeleton.h"
#include "ufbx.h"
#include "UfbxUtil.h"
#include "Gfx.h"        // DrawDebug 実装時に使う想定（今は未使用）

const std::vector<BoneInfo>& FbxSkeleton::Bones() const
{
	// TODO: return ステートメントをここに挿入します
}

const std::vector<DirectX::XMFLOAT4X4>& FbxSkeleton::CurrWorld() const
{
	// TODO: return ステートメントをここに挿入します
}

float FbxSkeleton::SceneRadius() const
{
	return 0.0f;
}

const std::unordered_map<const ufbx_node*, uint16_t>& FbxSkeleton::BoneIndexMap() const
{
	// TODO: return ステートメントをここに挿入します
}

const std::vector<DirectX::XMMATRIX>& FbxSkeleton::SkinMatrices() const
{
	// TODO: return ステートメントをここに挿入します
}

std::vector<DirectX::XMMATRIX>& FbxSkeleton::SkinMatrices()
{
	// TODO: return ステートメントをここに挿入します
}

bool FbxSkeleton::BuildFromScene(const ufbx_scene* scene)
{
	return false;
}

void FbxSkeleton::UpdateAtTime(const ufbx_scene* scene, const ufbx_anim* anim, double t_sec)
{
}

void FbxSkeleton::DrawDebug(const DirectX::XMMATRIX& world, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj)
{
}
