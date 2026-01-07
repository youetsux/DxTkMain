#pragma once
#include <DirectXMath.h>
#include <filesystem>
#include <unordered_map>
#include "ufbx.h"





namespace UfbxUtil
{
    namespace fs = std::filesystem;
    using namespace DirectX;
    // path / string utilities
    fs::path PathFromUtf8(const char* bytes, size_t len);
    fs::path PathFromUfbx(const ufbx_string& s);
    fs::path FileNameFromUfbx(const ufbx_string& s);
    bool     UfbxStringEquals(const ufbx_string& a, const ufbx_string& b);

    // attribute index helper

   template<typename AttrT>
    uint32_t ValueIndexOf(const AttrT& attr, uint32_t corner, uint32_t vtx)
    {

        if (attr.indices.count > 0)  return attr.indices.data[corner];

        if (attr.unique_per_vertex)  return vtx;

        return corner;
    }

    const ufbx_vertex_vec2* ResolveUVByName(
        const ufbx_mesh* mesh,
        const ufbx_string& uv_set_name);

    // matrix / vector helpers
    DirectX::XMFLOAT4X4 ToXMMatrix(const ufbx_matrix& m);

    XMVECTOR TransformPosition(const XMFLOAT3& p, const XMMATRIX& M);
    XMVECTOR TransformNormal(const XMFLOAT3& n, const XMMATRIX& M);

    DirectX::XMFLOAT3 TransformPoint(
        const DirectX::XMFLOAT4X4& M,
        const DirectX::XMFLOAT3& v);

    DirectX::XMFLOAT4X4 EvaluateNodeWorldRecursive(
        const ufbx_node* node,
        const ufbx_anim* anim,
        double           t,
        std::unordered_map<const ufbx_node*, DirectX::XMFLOAT4X4>& cache);

    const ufbx_texture* GetDiffuseTexture(const ufbx_material* mat);


}
