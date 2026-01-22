#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "FbxMesh.h"
#include "FbxSkeleton.h"

#include <filesystem>
#include <string>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cfloat>

#include <WICTextureLoader.h>
#include <Effects.h>
#include <CommonStates.h>

#include "Gfx.h"
#include "ufbx.h"
#include "UfbxUtil.h"

using Microsoft::WRL::ComPtr;


namespace FbxMeshBuild
{
	// NOTE:
	// - FbxMesh.cpp 内部専用の補助関数群をまとめる名前空間。
	// - FbxMesh クラスの private メンバに触れない「純粋な部品」に限定する。
	// - DirectXTK の WICTextureLoader は（環境によって）DeviceContext を取らない版があるため、
	//   ここでは "device だけ" のオーバーロードに合わせる。

	// ------------------------------------------------------------
	// FBX ファイルの置かれているディレクトリを返す
	// ------------------------------------------------------------
	// ------------------------------------------------------------
	// FBX ファイルの置かれているディレクトリを返す
	// ------------------------------------------------------------
	std::filesystem::path GetFbxDirectory(const char* fbx_path)
	{
		std::filesystem::path fbx_dir;

		if (fbx_path) {
			const size_t len = std::strlen(fbx_path);
			fbx_dir = UfbxUtil::PathFromUtf8(fbx_path, len).parent_path();
		}

		return fbx_dir;
	}

	// ------------------------------------------------------------
	// 基準 UV セットを選ぶ（ひとまず 1つ目の UV を採用）
	//   - mesh->vertex_uv があればそれを返す
	//   - 無ければ mesh->uv_sets[0].vertex_uv があればそれを返す
	//   - どちらも無ければ nullptr
	// ------------------------------------------------------------
	const ufbx_vertex_vec2* ChooseBaseUVSet(const ufbx_mesh* mesh)
	{
		if (!mesh) {
			return nullptr;
		}

		if (mesh->vertex_uv.exists) {
			return &mesh->vertex_uv;
		}

		if (mesh->uv_sets.count > 0 &&
			mesh->uv_sets.data[0].vertex_uv.exists)
		{
			return &mesh->uv_sets.data[0].vertex_uv;
		}

		return nullptr;
	}// ------------------------------------------------------------
// 面（face）をマテリアル単位に分類する
// ------------------------------------------------------------
	std::unordered_map<uint32_t, std::vector<uint32_t>> BuildFacesByMaterial(
		const ufbx_mesh* mesh)
	{
		std::unordered_map<uint32_t, std::vector<uint32_t>> faces_by_mat;
		if (!mesh) {
			return faces_by_mat;
		}

		for (uint32_t fi = 0; fi < (uint32_t)mesh->faces.count; ++fi) {
			uint32_t mi =
				(mesh->face_material.count > 0) ?
				mesh->face_material.data[fi] : 0;
			faces_by_mat[mi].push_back(fi);
		}

		return faces_by_mat;
	}

	// ------------------------------------------------------------
	// base_uv 選択と faces_by_mat 構築をまとめて行う
	//   - out_base_uv: ChooseBaseUVSet() の結果を返す
	//   - 戻り値     : BuildFacesByMaterial() の結果を返す
	// ------------------------------------------------------------
	std::unordered_map<uint32_t, std::vector<uint32_t>>
		BuildFacesByMaterialAndChooseBaseUV(
			const ufbx_mesh* mesh,
			const ufbx_vertex_vec2*& out_base_uv)
	{
		out_base_uv = ChooseBaseUVSet(mesh);
		return BuildFacesByMaterial(mesh);
	}

	// ------------------------------------------------------------
	// マテリアル取得（node 優先、無ければ mesh）
	// ------------------------------------------------------------
	const ufbx_material* ResolveMaterialForPart(
		const ufbx_node* node,
		const ufbx_mesh* mesh,
		uint32_t mat_index)
	{
		const ufbx_material* mat = nullptr;
		if (node &&
			node->materials.count > mat_index &&
			node->materials.data[mat_index])
		{
			mat = node->materials.data[mat_index];
		}
		else if (mesh &&
			mesh->materials.count > mat_index)
		{
			mat = mesh->materials.data[mat_index];
		}
		return mat;
	}


	// CommonStates / BasicEffect を必要なら生成する
	// ------------------------------------------------------------
	void EnsureStatesAndEffect(
		ID3D11Device* device,
		std::unique_ptr<DirectX::DX11::CommonStates>& states,
		std::unique_ptr<DirectX::DX11::BasicEffect>& fx)
	{
		if (!states) {
			states.reset(new DirectX::DX11::CommonStates(device));
		}
		if (!fx) {
			fx.reset(new DirectX::DX11::BasicEffect(device));
		}
	}

	// ------------------------------------------------------------
	// BasicEffect の既定パラメータを設定する
	// ------------------------------------------------------------
	void ConfigureDefaultBasicEffect(DirectX::DX11::BasicEffect* fx)
	{
		if (!fx) return;

		// 照明あり / 1 つ目のライトを有効化
		fx->SetLightingEnabled(true);
		fx->SetPerPixelLighting(true);

		// 頂点カラーは使わない（PNT2 のみ）
		fx->SetVertexColorEnabled(false);

		// テクスチャは MeshPart の SRV がある場合のみ有効化する
		fx->SetTextureEnabled(true);

		// 環境光・拡散色（最低限見えるように固定値）
		fx->SetAmbientLightColor({ 0.3f, 0.3f, 0.3f });
		fx->SetDiffuseColor({ 1.0f, 1.0f, 1.0f, 1.0f });

		// ライト 0
		fx->SetLightEnabled(0, true);
		fx->SetLightDirection(0, { -0.5f, -1.0f, 0.3f });
		fx->SetLightDiffuseColor(0, { 1.0f, 1.0f, 1.0f, 1.0f });
	}

	// ------------------------------------------------------------
	// InputLayout を必要なら作成する（PNT2 用）
	// ------------------------------------------------------------
	bool EnsureInputLayout(
		ID3D11Device* device,
		DirectX::DX11::BasicEffect* fx,
		Microsoft::WRL::ComPtr<ID3D11InputLayout>& layout)
	{
		if (layout) {
			return true;
		}
		if (!device || !fx) {
			return false;
		}

		const void* bc = nullptr;
		size_t      sz = 0;
		fx->GetVertexShaderBytecode(&bc, &sz);

		D3D11_INPUT_ELEMENT_DESC il[] =
		{
			{ "SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			  (UINT)offsetof(FbxMesh::VertexPNT2, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },

			{ "NORMAL",      0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			  (UINT)offsetof(FbxMesh::VertexPNT2, nrm), D3D11_INPUT_PER_VERTEX_DATA, 0 },

			{ "TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT,    0,
			  (UINT)offsetof(FbxMesh::VertexPNT2, uv),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		HRESULT hr = device->CreateInputLayout(
			il, 3, bc, sz, layout.ReleaseAndGetAddressOf());

		return SUCCEEDED(hr);
	}

	// ------------------------------------------------------------
	// 1 パーツ分のテクスチャ SRV を作る
	//   - FBX 内に埋め込みがあればメモリから
	//   - 外部参照なら fbx_dir から相対パスで探す
	//   - 失敗時は out_srv を Reset() して「テクスチャ無し」で描画を継続する
	// ------------------------------------------------------------
	void BuildTextureForPart(
		ID3D11Device* device,
		const std::filesystem::path& fbx_dir,
		const ufbx_material* mat,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& out_srv)
	{
		out_srv.Reset();

		if (!device) {
			return;
		}

		const ufbx_texture* tex = UfbxUtil::GetDiffuseTexture(mat);
		if (!tex) {
			return;
		}

		HRESULT hr = E_FAIL;

		// 1) FBX 埋め込みテクスチャ（メモリ）
		if (tex->content.size > 0 && tex->content.data) {
			hr = DirectX::CreateWICTextureFromMemory(
				device,
				reinterpret_cast<const uint8_t*>(tex->content.data),
				tex->content.size,
				nullptr,
				out_srv.ReleaseAndGetAddressOf());
		}
		// 2) 外部参照テクスチャ（ファイル）
		else if (tex->filename.length > 0 && tex->filename.data) {
			const std::filesystem::path tex_path =
				fbx_dir / UfbxUtil::FileNameFromUfbx(tex->filename);

			if (std::filesystem::exists(tex_path)) {
				hr = DirectX::CreateWICTextureFromFile(
					device,
					tex_path.wstring().c_str(),
					nullptr,
					out_srv.ReleaseAndGetAddressOf());
			}
		}

		if (FAILED(hr)) {
			out_srv.Reset();
		}
	}

} // namespace FbxMeshBuild



//------------------------------------------------------------
// BuildInternal 用の一時コンテキスト
//------------------------------------------------------------
struct BuildContext
{
	const ufbx_mesh* mesh = nullptr;
	const ufbx_vertex_vec2* uvv = nullptr;
	const std::vector<FbxMesh::VertexInfluence>* infl_per_vtx = nullptr;

	// ノードのジオメトリ変換（geometry_to_world）。無ければ Identity。
	DirectX::XMMATRIX geo = DirectX::XMMatrixIdentity();
};

namespace FbxMeshBuild
{
	// ExpandNode 用: 単一ノードを配列化する（挙動不変のまま整理）
	inline std::vector<const ufbx_node*> MakeSingleNodeList(const ufbx_node* node)
	{
		std::vector<const ufbx_node*> nodes;
		nodes.push_back(node);
		return nodes;
	}

	// ExpandAllNodes 用: scene の全ノードを配列化する（挙動不変のまま整理）
	inline std::vector<const ufbx_node*> MakeSceneNodeList(const ufbx_scene* scene)
	{
		std::vector<const ufbx_node*> nodes;
		nodes.reserve(scene->nodes.count);

		for (size_t i = 0; i < scene->nodes.count; ++i) {
			nodes.push_back(scene->nodes.data[i]);
		}
		return nodes;
	}


	// ExpandNodesImpl 用: mesh 展開結果をクリアする（挙動不変）
	inline void ClearExpandedMeshData(FbxMesh::MeshData& mesh)
	{
		mesh.vertices_.clear();
		mesh.indices_.clear();
		mesh.parts_.clear();
		mesh.influences_.clear();
		mesh.bind_vertices_.clear();
		mesh.skinned_vertices_.clear();
	}

	// ExpandNodesImpl 用: 境界情報をリセットする（挙動不変）
	inline void ResetBounds(BVolume& bounds)
	{
		bounds.Reset();
	}


	// ExpandNodesImpl 用: Skeleton 側の「ボーンノード→インデックス」辞書を参照する（型を固定しない）
	inline decltype(auto) GetBoneIndexMap(FbxSkeleton& skeleton)
	{
		return skeleton.Data().bone_index_of_;
	}

	// ExpandNodesImpl 用: スキニング有無フラグをリセットする（挙動不変）
	inline void ResetHasSkinningFlag(bool& has_skinning)
	{
		has_skinning = false;
	}

	// ExpandNodesImpl 用: シーン半径を書き込む（挙動不変）
	inline void WriteSceneRadiusIfNeeded(
		FbxSkeleton& skeleton,
		float radius,
		bool write_scene_radius)
	{
		if (write_scene_radius) {
			skeleton.Data().scene_radius_ = radius;
		}
	}

	// ExpandNodesImpl 用: BuildContext を初期化する（挙動不変）
	inline BuildContext MakeBuildContext(
		const ufbx_node* node,
		const ufbx_mesh* mesh,
		const ufbx_vertex_vec2* uvv,
		const std::vector<FbxMesh::VertexInfluence>* infl_per_vtx,
		bool apply_geo)
	{
		BuildContext ctx;
		ctx.mesh = mesh;
		ctx.uvv = uvv;
		ctx.infl_per_vtx = infl_per_vtx;

		// ExpandNode と同じ挙動：node の geometry_to_world を適用
		if (apply_geo && node) {
			DirectX::XMFLOAT4X4 mtx = UfbxUtil::ToXMMatrix(node->geometry_to_world);
			ctx.geo = DirectX::XMLoadFloat4x4(&mtx);
		}
		return ctx;
	}



	// ExpandNodesImpl 用: バインド頂点/スキン頂点を初期化する（挙動不変）
	inline void InitBindAndSkinnedVertices(FbxMesh::MeshData& mesh)
	{
		mesh.bind_vertices_ = mesh.vertices_;
		mesh.skinned_vertices_ = mesh.vertices_;
	}




}


namespace
{
	using namespace DirectX;
	namespace fs = std::filesystem;

	// ------------------------------------------------------------
	// ボーンウェイト（weight）降順ソート用
	// ------------------------------------------------------------
	bool CompareBoneWeightPair(
		const std::pair<uint16_t, float>& a,
		const std::pair<uint16_t, float>& b)
	{
		return a.second > b.second;
	}

	// ------------------------------------------------------------
	// メッシュのスキニング情報から「頂点ごとの影響（最大4）」を作る
	// ------------------------------------------------------------
	void BuildInfluencesForMesh(
		const ufbx_mesh* mesh,
		const std::unordered_map<const ufbx_node*, uint16_t>& bone_index_map,
		std::vector<FbxMesh::VertexInfluence>& infl_per_vtx)
	{
		infl_per_vtx.clear();

		if (!mesh || mesh->skin_deformers.count == 0) {
			return;
		}

		infl_per_vtx.assign(mesh->num_vertices, FbxMesh::VertexInfluence());

		const ufbx_skin_deformer* skin = mesh->skin_deformers.data[0];
		const ufbx_skin_cluster_list& clusters = skin->clusters;
		const ufbx_skin_vertex_list& vtx_list = skin->vertices;
		const ufbx_skin_weight_list& w_list = skin->weights;

		// 各頂点ごとに (boneIndex, weight) を一旦すべて集計（後で上位4つに制限）
		std::vector<std::vector<std::pair<uint16_t, float>>> acc(mesh->num_vertices);

		for (size_t v = 0; v < vtx_list.count; ++v) {
			const ufbx_skin_vertex sv = vtx_list.data[v];
			uint32_t begin = sv.weight_begin;
			uint32_t n = sv.num_weights;

			for (uint32_t k = 0; k < n; ++k) {
				const ufbx_skin_weight w = w_list.data[begin + k];
				uint32_t ci = w.cluster_index;
				if (ci >= clusters.count) continue;

				const ufbx_skin_cluster* cl = clusters.data[ci];
				if (!cl || !cl->bone_node) continue;

				auto it = bone_index_map.find(cl->bone_node);
				if (it == bone_index_map.end()) continue;

				acc[v].push_back(std::make_pair(it->second, (float)w.weight));
			}
		}

		// 4 {܂łɍiĐK
		for (size_t v = 0; v < acc.size(); ++v) {
			auto& list = acc[v];
			if (list.empty()) continue;

			std::sort(list.begin(), list.end(), CompareBoneWeightPair);

			if (list.size() > 4) {
				list.resize(4);
			}

			float sum = 0.0f;
			for (auto& p : list) sum += p.second;
			if (sum <= 0.0f) sum = 1.0f;

			FbxMesh::VertexInfluence vi;
			size_t n_infl = list.size();
			if (n_infl > 4) n_infl = 4;

			for (size_t i = 0; i < n_infl; ++i) {
				vi.bone[i] = list[i].first;
				vi.weight[i] = list[i].second / sum;
			}
			infl_per_vtx[v] = vi;
		}
	}

	// ------------------------------------------------------------
	// マテリアルのテクスチャが指定する UV セットを選ぶ（無ければ base_uv）
	// ------------------------------------------------------------
	const ufbx_vertex_vec2* ChooseUVSet(
		const ufbx_mesh* mesh,
		const ufbx_material* mat,
		const ufbx_vertex_vec2* base_uv)
	{
		if (!mesh) return base_uv;

		const ufbx_texture* tex_for_uv = UfbxUtil::GetDiffuseTexture(mat);
		const ufbx_vertex_vec2* uvv = base_uv;

		if (tex_for_uv && tex_for_uv->uv_set.length > 0) {
			const ufbx_vertex_vec2* alt =
				UfbxUtil::ResolveUVByName(mesh, tex_for_uv->uv_set);
			if (alt && alt->exists) {
				uvv = alt;
			}
		}
		return uvv;
	}
} // anonymous namespace

FbxMesh::FbxMesh() = default;
FbxMesh::~FbxMesh() = default;

//================================================================
// FbxMesh::EmitCorner
//================================================================
void FbxMesh::EmitCorner(
	BuildContext& ctx,
	uint32_t corner,
	uint32_t vtx)
{
	using namespace DirectX;

	const ufbx_mesh* mesh = ctx.mesh;

	// 位置（position）
	uint32_t pi = UfbxUtil::ValueIndexOf(mesh->vertex_position, corner, vtx);
	ufbx_vec3 p = mesh->vertex_position.values.data[pi];
	XMFLOAT3 P((float)p.x, (float)p.y, (float)p.z);

	// ノードのジオメトリ変換を位置に適用
	{
		DirectX::XMVECTOR pv = UfbxUtil::TransformPosition(P, ctx.geo);
		DirectX::XMStoreFloat3(&P, pv);
	}

	// AABB XV
	bounds_.WrapBox(P);

	// 法線（normal）
	XMFLOAT3 N(0.0f, 1.0f, 0.0f);
	if (mesh->vertex_normal.exists) {
		uint32_t ni2 = UfbxUtil::ValueIndexOf(mesh->vertex_normal, corner, vtx);
		ufbx_vec3 n = mesh->vertex_normal.values.data[ni2];

		double len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
		if (len > 0.0) {
			double inv = 1.0 / len;
			N.x = (float)(n.x * inv);
			N.y = (float)(n.y * inv);
			N.z = (float)(n.z * inv);
		}
		// ノード transform を法線にも反映（平行移動は無視）
		if (ctx.mesh) {
			DirectX::XMVECTOR nv = UfbxUtil::TransformNormal(N, ctx.geo);
			nv = DirectX::XMVector3Normalize(nv);
			DirectX::XMStoreFloat3(&N, nv);
		}

	}

	// UV 取得（V は上下反転して DirectX の慣習に合わせる）
	XMFLOAT2 T(0.0f, 0.0f);
	if (ctx.uvv && ctx.uvv->exists) {
		uint32_t ti = UfbxUtil::ValueIndexOf(*ctx.uvv, corner, vtx);
		ufbx_vec2 t = ctx.uvv->values.data[ti];
		T.x = (float)t.x;
		T.y = (float)(1.0 - t.y);
	}

	VertexPNT2 vtx_out;
	vtx_out.pos = P;
	vtx_out.nrm = N;
	vtx_out.uv = T;

	VertexInfluence vi{};
	if (ctx.infl_per_vtx &&
		!ctx.infl_per_vtx->empty() &&
		vtx < ctx.infl_per_vtx->size())
	{
		vi = (*ctx.infl_per_vtx)[vtx];
	}

	mesh_.influences_.push_back(vi);
	mesh_.indices_.push_back((uint32_t)mesh_.vertices_.size());
	mesh_.vertices_.push_back(vtx_out);
}



//================================================================
// FbxMesh::EmitTriangleFanFaces
//   face_list を triangle fan で三角形化し、EmitCorner() で頂点を吐く
//   ※挙動・順序は ExpandNodesImpl 内の旧実装と同一
//================================================================
void FbxMesh::EmitTriangleFanFaces(
	BuildContext& ctx,
	const ufbx_mesh* mesh,
	const std::vector<uint32_t>& face_list)
{
	if (!mesh) return;

	for (uint32_t f_index : face_list) {
		const ufbx_face f = mesh->faces.data[f_index];
		if (f.num_indices < 3) continue;

		// 先頭頂点を固定して (0, k+1, k+2) の三角形に分割
		for (uint32_t k = 0; k + 2 < f.num_indices; ++k) {
			uint32_t corners[3] = {
				f.index_begin + 0,
				f.index_begin + (k + 1),
				f.index_begin + (k + 2),
			};

			for (int c = 0; c < 3; ++c) {
				uint32_t corner = corners[c];
				uint32_t vtx = mesh->vertex_indices.data[corner];
				EmitCorner(ctx, corner, vtx);
			}
		}
	}
}

//================================================================
// BuildFromScene
//================================================================
bool FbxMesh::BuildFromScene(const ufbx_scene* scene,
	FbxSkeleton& skeleton,
	const char* fbx_path)
{
	if (!scene) return false;

	ExpandAllNodes(scene, skeleton);

	if (!CreateGpuBuffers()) {
		return false;
	}

	if (!CreateEffectsAndTextures(fbx_path, scene)) {
		return false;
	}

	// GPU バッファ生成後は CPU 側の頂点配列を破棄（スキニング用は別配列で保持）
	mesh_.vertices_.clear();
	mesh_.vertices_.shrink_to_fit();

	return true;
}

//================================================================
// BuildFromNode (node->mesh 1個だけを CPU 展開)
//   ※用途：1ノードだけ読みたい場合
//================================================================
bool FbxMesh::BuildFromNode(const ufbx_scene* scene,
	const ufbx_node* node,
	FbxSkeleton& skeleton,
	const char* fbx_path)
{
	if (!scene || !node || !node->mesh) return false;

	ExpandNode(scene, node, skeleton);

	if (!CreateGpuBuffers()) {
		return false;
	}

	if (!CreateEffectsAndTextures(fbx_path, scene)) {
		return false;
	}

	// GPU バッファ生成後は CPU 側の頂点配列を破棄（スキニング用は別配列で保持）
	mesh_.vertices_.clear();
	mesh_.vertices_.shrink_to_fit();

	return true;
}

//================================================================
// ExpandNode (CPU 展開): node 1つのメッシュを展開
//   ※用途：1ノードだけ読みたい場合
//================================================================
//================================================================
// ExpandNode (CPU): node 1つだけ展開
//================================================================
//================================================================
// ExpandNodesImpl (CPU): ExpandNode / ExpandAllNodes 共通
//   apply_geo:
//     true  -> node->geometry_to_world を頂点/法線へ適用（ExpandNode と同じ）
//     false -> 適用しない（ExpandAllNodes と同じ）
//   write_scene_radius:
//     true  -> skeleton.Data().scene_radius_ を bounds_.radius で更新（ExpandAllNodes と同じ）
//================================================================
//================================================================
// PrepareSkinningForMeshImpl
//   - メッシュのスキニング有無を反映し、頂点ごとの影響（最大4）を構築する
//   - ※挙動は ExpandNodesImpl 内の旧処理と同一
//================================================================
void FbxMesh::PrepareSkinningForMeshImpl(
	const ufbx_mesh* mesh,
	const std::unordered_map<const ufbx_node*, uint16_t>& bone_index_map,
	std::vector<VertexInfluence>& out_infl_per_vtx)
{
	// スキニングの有無（シーン全体として1つでもあれば true）
	if (mesh && mesh->skin_deformers.count > 0) {
		has_skinning_ = true;
	}

	// 頂点ごとの影響（最大4）
	BuildInfluencesForMesh(mesh, bone_index_map, out_infl_per_vtx);
}

// ------------------------------------------------------------
// ExpandNodesImpl の後処理（境界確定 / scene_radius 更新 / bind&skinned 初期化）
// ------------------------------------------------------------
void FbxMesh::FinalizeExpandImpl(FbxSkeleton& skeleton, bool write_scene_radius)
{
	// ------------------------------------------------------------
	// [Finalize] (ExpandNodesImpl)
	//   - Finalizes bounds (AABB -> sphere)
	//   - Optionally writes scene radius to skeleton (ExpandAllNodes behavior)
	//   - Initializes bind/skinned vertex buffers from expanded vertices
	// Notes:
	//   - Does NOT expand any node/mesh. Expansion already completed.
	// ------------------------------------------------------------
	// 4) 境界（AABB→Sphere）を確定
	bounds_.RecalcSphereFromAABB();

	// ExpandAllNodes と同じ挙動：シーン半径を skeleton 側に保存
	FbxMeshBuild::WriteSceneRadiusIfNeeded(skeleton, bounds_.radius, write_scene_radius);

	// 5) スキニング用のバインド/作業頂点を初期化
	FbxMeshBuild::InitBindAndSkinnedVertices(mesh_);
}


void FbxMesh::ExpandSingleNodeImpl(
	const ufbx_node* node,
	const std::unordered_map<const ufbx_node*, uint16_t>& bone_index_map,
	bool apply_geo)
{
	if (!node) return;

	const ufbx_mesh* mesh = node->mesh;
	if (!mesh) return;

	// -----------------------------------------
	// 3-1) 頂点ごとの影響（ボーン/ウェイト）を構築
	// -----------------------------------------
	std::vector<VertexInfluence> infl_per_vtx;
	PrepareSkinningForMeshImpl(mesh, bone_index_map, infl_per_vtx);

	// -----------------------------------------
	// 3-2) UV セット（基本は最初のUV）
	// -----------------------------------------
	const ufbx_vertex_vec2* base_uv = nullptr;

	// -----------------------------------------
	// 3-3) 面をマテリアル単位に分類
	// -----------------------------------------
	auto faces_by_mat = FbxMeshBuild::BuildFacesByMaterialAndChooseBaseUV(mesh, base_uv);

	// -----------------------------------------
	// 3-4) マテリアルごとに MeshPart を作って頂点を吐く
	// -----------------------------------------
	ExpandAllMaterialGroupsImpl(
		node,
		mesh,
		faces_by_mat,
		infl_per_vtx,
		base_uv,
		apply_geo);
}

void FbxMesh::ExpandMaterialGroupImpl(
	const ufbx_node* node,
	const ufbx_mesh* mesh,
	uint32_t mat_index,
	const std::vector<uint32_t>& face_list,
	const std::vector<VertexInfluence>& infl_per_vtx,
	const ufbx_vertex_vec2* base_uv,
	bool apply_geo)
{
	MeshPart part;
	part.mat = nullptr;
	part.start_index = (uint32_t)mesh_.indices_.size();

	part.mat = FbxMeshBuild::ResolveMaterialForPart(node, mesh, mat_index);

	// テクスチャが指定する UV セットを選択
	const ufbx_vertex_vec2* uvv =
		ChooseUVSet(mesh, part.mat, base_uv);

	// EmitCorner() に渡すコンテキスト
	BuildContext ctx =
		FbxMeshBuild::MakeBuildContext(node, mesh, uvv, &infl_per_vtx, apply_geo);

	// face -> triangle fan で三角形化して吐く
	EmitTriangleFanFaces(ctx, mesh, face_list);
	CommitMeshPartIfNotEmpty(part);
}

void FbxMesh::CommitMeshPartIfNotEmpty(MeshPart& part)
{
	// MeshPart 確定（インデックス範囲）
	part.index_count =
		(uint32_t)mesh_.indices_.size() - part.start_index;
	if (part.index_count > 0) {
		mesh_.parts_.push_back(part);
	}
}




void FbxMesh::ExpandAllMaterialGroupsImpl(
	const ufbx_node* node,
	const ufbx_mesh* mesh,
	const std::unordered_map<uint32_t, std::vector<uint32_t>>& faces_by_mat,
	const std::vector<VertexInfluence>& infl_per_vtx,
	const ufbx_vertex_vec2* base_uv,
	bool apply_geo)
{
	for (auto& kv : faces_by_mat) {
		uint32_t mat_index = kv.first;
		const std::vector<uint32_t>& face_list = kv.second;

		ExpandMaterialGroupImpl(
			node,
			mesh,
			mat_index,
			face_list,
			infl_per_vtx,
			base_uv,
			apply_geo);
	}
}


void FbxMesh::ExpandNodesImpl(
	const ufbx_scene* scene,
	const std::vector<const ufbx_node*>& nodes,
	FbxSkeleton& skeleton,
	bool apply_geo,
	bool write_scene_radius)
{
	using namespace DirectX;

	if (!scene) return;


	// ------------------------------------------------------------
	// [Prepare] Clear per-expand CPU data and reset bounds/skinning flags
	// ------------------------------------------------------------
	PrepareExpandImpl();

	const auto& sk_data = skeleton.Data();
	const auto& bone_index_map = sk_data.bone_index_of_;


	// ------------------------------------------------------------
	// [Expand ] Expand nodes -> vertices / indices / parts
	// ------------------------------------------------------------
	ExpandAllNodesImpl(nodes, bone_index_map, apply_geo);

	// ------------------------------------------------------------
	// [Finalize] Recalc bounds / write scene radius / init skinning data
	// ------------------------------------------------------------
	FinalizeExpandImpl(skeleton, write_scene_radius);
}
//================================================================
// PrepareExpandImpl (CPU): ExpandNodesImpl の準備工程をまとめる
//   - 前回データクリア
//   - 境界リセット
//   - ボーン辞書参照取得
//   - has_skinning_ 初期化
//================================================================
void FbxMesh::PrepareExpandImpl()
{
	// ------------------------------------------------------------
	// [Prepare] (ExpandNodesImpl)
	//   - Clears previous CPU-side expanded mesh data
	//   - Resets bounds and per-expand flags (has_skinning_)
	// Notes:
	//   - Does NOT expand any node/mesh. Expansion happens in later steps.
	//   - Does NOT touch skeleton contents (read-only outside).
	// ------------------------------------------------------------
	// 1) 前回データをクリア
	FbxMeshBuild::ClearExpandedMeshData(mesh_);

	// 2) 境界情報をリセット
	FbxMeshBuild::ResetBounds(bounds_);

	// スキニングフラグを初期化
	FbxMeshBuild::ResetHasSkinningFlag(has_skinning_);
}



//================================================================
// ExpandAllNodesImpl (CPU): nodes 配列を走査して各ノードを展開
//================================================================
void FbxMesh::ExpandAllNodesImpl(
	const std::vector<const ufbx_node*>& nodes,
	const std::unordered_map<const ufbx_node*, uint16_t>& bone_index_map,
	bool apply_geo)
{
	for (const ufbx_node* node : nodes) {
		ExpandSingleNodeImpl(node, bone_index_map, apply_geo);
	}
}


void FbxMesh::ExpandNode(const ufbx_scene* scene,
	const ufbx_node* node,
	FbxSkeleton& skeleton)
{
	// ExpandNode は geometry_to_world を適用する
	const std::vector<const ufbx_node*> nodes = FbxMeshBuild::MakeSingleNodeList(node);
	ExpandNodesImpl(scene, nodes, skeleton, true, false);
}


//================================================================
// bVWJiCPUj
//================================================================
//================================================================
// ExpandAllNodes (CPU): scene 内の全メッシュノードを展開
//================================================================
void FbxMesh::ExpandAllNodes(const ufbx_scene* scene,
	FbxSkeleton& skeleton)

{
	if (!scene) return;

	std::vector<const ufbx_node*> nodes = FbxMeshBuild::MakeSceneNodeList(scene);

	// ExpandAllNodes は geometry_to_world を適用しない（従来どおり）
	// さらに scene_radius_ を更新する（従来どおり）
	ExpandNodesImpl(scene, nodes, skeleton, false, true);
}


//================================================================
//================================================================
// GPU バッファ作成
//================================================================
//================================================================
bool FbxMesh::CreateGpuBuffers()
{
	if (mesh_.vertices_.empty()) return false;

	ID3D11Device* device = Gfx::Dev();
	if (!device) return false;

	// 頂点バッファ（VB）作成
	{
		D3D11_BUFFER_DESC desc;
		std::memset(&desc, 0, sizeof(desc));
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.ByteWidth =
			(UINT)(mesh_.vertices_.size() * sizeof(VertexPNT2));
		desc.Usage = D3D11_USAGE_DEFAULT;

		D3D11_SUBRESOURCE_DATA init;
		std::memset(&init, 0, sizeof(init));
		init.pSysMem = &mesh_.vertices_[0];

		HRESULT hr = device->CreateBuffer(
			&desc, &init, vb_.GetAddressOf());
		if (FAILED(hr)) {
			return false;
		}
	}

	// インデックスバッファ（IB）作成
	{
		D3D11_BUFFER_DESC desc;
		std::memset(&desc, 0, sizeof(desc));
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		desc.ByteWidth =
			(UINT)(mesh_.indices_.size() * sizeof(uint32_t));
		desc.Usage = D3D11_USAGE_DEFAULT;

		D3D11_SUBRESOURCE_DATA init;
		std::memset(&init, 0, sizeof(init));
		init.pSysMem = &mesh_.indices_[0];

		HRESULT hr = device->CreateBuffer(
			&desc, &init, ib_.GetAddressOf());
		if (FAILED(hr)) {
			return false;
		}
	}

	return true;
}

bool FbxMesh::CreateEffectsAndTextures(
	const char* fbx_path,
	const ufbx_scene* /*scene*/)
{
	//================================================================
	// CreateEffectsAndTextures
	//
	// 役割:
	//   1) BasicEffect / CommonStates / InputLayout の準備
	//   2) MeshPart ごとのテクスチャ(SRV)の読み込み
	//
	// 方針:
	//   - device/context は Gfx から取得する（引数では渡さない）
	//   - テクスチャ読み込みに失敗しても、そのパーツだけ無地で描画を継続する
	//================================================================


	// [1] D3D デバイス/コンテキスト取得
	ID3D11Device* device = Gfx::Dev();
	if (!device) {
		return false;
	}

	using namespace FbxMeshBuild;



	// [2] CommonStates / BasicEffect を必要なら生成（再利用する）
	FbxMeshBuild::EnsureStatesAndEffect(device, states_, fx_);


	// [3] BasicEffect の既定パラメータを設定（固定値）
	FbxMeshBuild::ConfigureDefaultBasicEffect(fx_.get());


	// [4] InputLayout を必要なら作成
	if (!FbxMeshBuild::EnsureInputLayout(device, fx_.get(), layout_)) {
		return false;
	}


	// [5] 外部テクスチャ探索用に、FBX ファイルのディレクトリを取得
	const std::filesystem::path fbx_dir = FbxMeshBuild::GetFbxDirectory(fbx_path);


	// [6] MeshPart ごとにテクスチャ(SRV)を構築
	for (auto& part : mesh_.parts_) {
		FbxMeshBuild::BuildTextureForPart(device, fbx_dir, part.mat, part.srv);
	}

	return true;
}


//================================================================
//================================================================
// エフェクト・テクスチャ作成
//================================================================
//================================================================

//================================================================
// CPU スキニング（頂点ごとの影響とスキン行列からスキン後頂点を生成）
//   - bind_vertices_ : バインド姿勢の頂点（入力）
//   - influences_    : 各頂点の (bone[4], weight[4])
//   - skin_mats      : ボーンごとのスキン行列（geom_bind_world * current_world）
//   - skinned_vertices_ : 出力（描画前に VB に UpdateSubresource で反映）
//================================================================

void FbxMesh::ApplySkinCPU(
	const std::vector<DirectX::XMMATRIX>& skin_mats)
{
	using namespace DirectX;

	const auto& influences = mesh_.influences_;
	const auto& bind_vertices = mesh_.bind_vertices_;
	auto& out_vertices = mesh_.skinned_vertices_;

	// 入力・出力サイズを揃える

	size_t n = bind_vertices.size();
	if (n == 0) return;

	if (out_vertices.size() != n) {
		out_vertices.resize(n);
	}
	if (influences.size() < n) {
		// influences_ が頂点数より短い場合は安全のため短い方に合わせる
		n = influences.size();
	}


	// 各頂点について、最大4ボーンの線形ブレンドで位置・法線を計算する
	for (size_t v = 0; v < n; ++v) {
		const VertexInfluence& inf = influences[v];

		XMVECTOR P = XMVectorZero();
		XMVECTOR N = XMVectorZero();
		bool any = false;

		for (int k = 0; k < 4; ++k) {
			// (bone[k], weight[k]) が有効なら、そのボーン行列で変換して加算
			float    w = inf.weight[k];
			uint16_t b = inf.bone[k];

			if (w <= 0.0f) continue;
			if (b >= skin_mats.size()) continue;

			const XMMATRIX& B = skin_mats[b];
			XMVECTOR W = XMVectorReplicate(w);

			P = XMVectorMultiplyAdd(
				UfbxUtil::TransformPosition(bind_vertices[v].pos, B),
				W, P);

			N = XMVectorMultiplyAdd(
				UfbxUtil::TransformNormal(bind_vertices[v].nrm, B),
				W, N);

			any = true;
		}


		// 影響が無い頂点は、そのままバインド姿勢をコピー
		if (!any) {
			out_vertices[v] = bind_vertices[v];
			continue;
		}

		VertexPNT2 sv = bind_vertices[v];
		XMStoreFloat3(&sv.pos, P);
		// 法線は合成後に正規化してから格納
		N = XMVector3Normalize(N);
		XMStoreFloat3(&sv.nrm, N);
		out_vertices[v] = sv;
	}
}

//================================================================
// Draw
//   VB/IB と BasicEffect を使って MeshPart 単位で描画する。
//   スキニングが有効な場合は、CPU でスキニング→VB を UpdateSubresource で更新して描画する。
//================================================================

//================================================================
// Draw helpers（ラムダ無しで工程分割）
//================================================================

//------------------------------------------------------------
// Draw できる状態か（必要なリソースが揃っているか）を確認
//------------------------------------------------------------
bool FbxMesh::ValidateDrawResources(ID3D11DeviceContext* ctx) const
{
	if (!ctx) return false;

	// GPU バッファ
	if (!vb_ || !ib_) return false;

	// Effect / States / InputLayout
	if (!fx_ || !states_ || !layout_) return false;

	// インデックスが無ければ描けない
	if (mesh_.indices_.empty()) return false;

	return true;
}

//------------------------------------------------------------
// スキニング（CPU）を必要なら更新し、頂点バッファに反映する
//------------------------------------------------------------
void FbxMesh::UpdateSkinningIfNeeded(
	ID3D11DeviceContext* ctx,
	FbxSkeleton& skeleton)
{
	if (!has_skinning_) return;

	// 影響情報が無い / バインド頂点が無いなら更新できない
	if (mesh_.influences_.empty()) return;
	if (mesh_.bind_vertices_.empty()) return;

	// ボーン数に合わせてスキン行列配列を準備
	auto& skin_mats = skeleton.SkinMatrices();
	skin_mats.resize(skeleton.Bones().size());

	// 各ボーンのスキン行列を作る（geom_bind_world と 現在ワールドの合成）
	for (size_t i = 0; i < skeleton.Bones().size(); ++i) {
		DirectX::XMMATRIX W = DirectX::XMLoadFloat4x4(&skeleton.CurrWorld()[i]);
		DirectX::XMMATRIX G2B = DirectX::XMLoadFloat4x4(&skeleton.Bones()[i].geom_bind_world);
		skin_mats[i] = DirectX::XMMatrixMultiply(G2B, W);
	}

	// CPU スキニング実行（bind -> skinned を更新）
	ApplySkinCPU(skin_mats);

	// 結果を GPU の頂点バッファに反映
	ctx->UpdateSubresource(
		vb_.Get(), 0, nullptr,
		&mesh_.skinned_vertices_[0], 0, 0);
}

//------------------------------------------------------------
// 入力アセンブラ・固定ステートなどの描画準備を行う
//------------------------------------------------------------
void FbxMesh::SetupPipelineForMeshDraw(
	ID3D11DeviceContext* ctx,
	const DirectX::XMMATRIX& world,
	const DirectX::XMMATRIX& view,
	const DirectX::XMMATRIX& proj)
{
	// 入力レイアウト / VB / IB / プリミティブ形式
	UINT stride = sizeof(VertexPNT2);
	UINT offset = 0;
	ID3D11Buffer* vb = vb_.Get();

	ctx->IASetInputLayout(layout_.Get());
	ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
	ctx->IASetIndexBuffer(ib_.Get(), DXGI_FORMAT_R32_UINT, 0);
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 深度・カリングなどの固定ステート
	ctx->OMSetDepthStencilState(states_->DepthDefault(), 0);
	ctx->RSSetState(states_->CullCounterClockwise());

	// サンプラ
	ID3D11SamplerState* samp = states_->LinearClamp();
	ctx->PSSetSamplers(0, 1, &samp);

	// 行列（World/View/Proj）
	fx_->SetWorld(world);
	fx_->SetView(view);
	fx_->SetProjection(proj);
}

//------------------------------------------------------------
// MeshPart を順に描画する（BlendState を退避/復帰する）
//------------------------------------------------------------
void FbxMesh::DrawPartsWithBlendRestore(ID3D11DeviceContext* ctx)
{
	// 既存のブレンド設定を退避（ComPtr で自動 Release）
	Microsoft::WRL::ComPtr<ID3D11BlendState> prev_blend;
	FLOAT prev_factor[4];
	UINT  prev_mask = 0xFFFFFFFF;
	ctx->OMGetBlendState(prev_blend.GetAddressOf(), prev_factor, &prev_mask);

	ID3D11BlendState* bound = prev_blend.Get();

	// パーツごとにテクスチャ有無でブレンドを切り替え
	for (size_t i_part = 0; i_part < mesh_.parts_.size(); ++i_part) {
		const MeshPart& part = mesh_.parts_[i_part];

		const bool has_tex = (part.srv != nullptr);
		ID3D11BlendState* target =
			has_tex ? states_->NonPremultiplied()
			: states_->Opaque();

		if (bound != target) {
			ctx->OMSetBlendState(target, nullptr, 0xFFFFFFFF);
			bound = target;
		}

		// テクスチャ設定（無ければ無効化して描画）
		fx_->SetTextureEnabled(has_tex);
		if (has_tex) {
			fx_->SetTexture(part.srv.Get());
		}

		// シェーダ適用 & 描画
		fx_->Apply(ctx);
		ctx->DrawIndexed(part.index_count, part.start_index, 0);
	}

	// ブレンド設定を復帰
	ctx->OMSetBlendState(prev_blend.Get(), prev_factor, prev_mask);
}

void FbxMesh::Draw(
	const DirectX::XMMATRIX& world,
	const DirectX::XMMATRIX& view,
	const DirectX::XMMATRIX& proj,
	FbxSkeleton& skeleton)
{
	//============================================================
	// 目的:
	//   - FbxMesh が保持する VB/IB を BasicEffect で描画する
	//   - スキニングがある場合は CPU スキニング結果を VB に反映してから描く
	//
	// 方針:
	//   - 描画手順を工程ごとに分割し、見通しを良くする
	//   - ラムダは使わない（工程は名前付き関数にする）
	//============================================================

	// 0) D3D コンテキスト取得（描画に必須）
	ID3D11DeviceContext* ctx = Gfx::Ctx();

	if (!ValidateDrawResources(ctx)) {
		return;
	}

	// 1) スキニング更新（必要なときだけ）
	UpdateSkinningIfNeeded(ctx, skeleton);

	// 2) パイプライン設定（IA/RS/OM/Sampler + 行列）
	SetupPipelineForMeshDraw(ctx, world, view, proj);

	// 3) パーツ描画（BlendState 退避/復帰込み）
	DrawPartsWithBlendRestore(ctx);
}


//================================================================
// ApplyUniformScale
//   モデル全体を一様スケールする（CPU 側の保持データと境界情報を更新）
//   ※VB/IB をすでに作成済みの場合、非スキニングモデルは見た目に反映されない可能性がある。
//     （この関数は VB を更新しないため）
//================================================================
void FbxMesh::ApplyUniformScale(float s)
{
	if (s <= 0.0f) return;

	// CPU 側に保持している頂点データをすべてスケール（pos のみ）

	for (auto& v : mesh_.vertices_) {
		v.pos.x *= s; v.pos.y *= s; v.pos.z *= s;
	}
	for (auto& v : mesh_.bind_vertices_) {
		v.pos.x *= s; v.pos.y *= s; v.pos.z *= s;
	}
	for (auto& v : mesh_.skinned_vertices_) {
		v.pos.x *= s; v.pos.y *= s; v.pos.z *= s;
	}


	// 境界情報も同じ倍率で更新
	bounds_.Scale(s);
}

