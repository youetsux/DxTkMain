#include <DirectXMath.h>
#include <filesystem>
#include <unordered_map>
#include <cstring>

#include "ufbx.h"

namespace fs = std::filesystem;
using namespace DirectX;

namespace UfbxUtil
{
    // ------------------------------------------------------------
    // path / string utilities
    // ------------------------------------------------------------


    fs::path PathFromUtf8(const char* bytes, size_t len)
    {
        const char8_t* p = reinterpret_cast<const char8_t*>(bytes);
        return fs::path(std::u8string(p, p + len));
    }


    fs::path PathFromUfbx(const ufbx_string& s)
    {
        if (!s.data || s.length == 0) return fs::path();
        return PathFromUtf8(s.data, s.length);
    }


    fs::path FileNameFromUfbx(const ufbx_string& s)
    {
        return PathFromUfbx(s).filename();
    }



    bool UfbxStringEquals(const ufbx_string& a, const ufbx_string& b)
    {
        if (a.length != b.length || !a.data || !b.data) return false;
        return std::strncmp(a.data, b.data, a.length) == 0;
    }

    // ------------------------------------------------------------


    // ------------------------------------------------------------
    const ufbx_texture* GetDiffuseTexture(const ufbx_material* mat)
    {
        if (!mat) return nullptr;
        if (mat->pbr.base_color.texture)    return mat->pbr.base_color.texture;
        if (mat->fbx.diffuse_color.texture) return mat->fbx.diffuse_color.texture;
        if (mat->fbx.ambient_color.texture) return mat->fbx.ambient_color.texture;
        return nullptr;
    }

    // ------------------------------------------------------------



    // ------------------------------------------------------------
    const ufbx_vertex_vec2* ResolveUVByName(
        const ufbx_mesh* mesh,
        const ufbx_string& uv_set_name)
    {
        if (!mesh || uv_set_name.length == 0) return nullptr;

        for (size_t i = 0; i < mesh->uv_sets.count; ++i) {
            const ufbx_uv_set& us = mesh->uv_sets.data[i];
            if (us.vertex_uv.exists && UfbxStringEquals(us.name, uv_set_name)) {
                return &us.vertex_uv;
            }
        }
        return nullptr;
    }


    // ------------------------------------------------------------


    // ------------------------------------------------------------
    DirectX::XMFLOAT4X4 ToXMMatrix(const ufbx_matrix& m)
    {
        DirectX::XMFLOAT4X4 out;


        out._11 = (float)m.m00; out._12 = (float)m.m10; out._13 = (float)m.m20; out._14 = 0.0f;
        out._21 = (float)m.m01; out._22 = (float)m.m11; out._23 = (float)m.m21; out._24 = 0.0f;
        out._31 = (float)m.m02; out._32 = (float)m.m12; out._33 = (float)m.m22; out._34 = 0.0f;
        out._41 = (float)m.m03; out._42 = (float)m.m13; out._43 = (float)m.m23; out._44 = 1.0f;

        return out;
    }


    XMVECTOR TransformPosition(const XMFLOAT3& p, const XMMATRIX& M)
    {
        return XMVector3Transform(XMLoadFloat3(&p), M);
    }


    XMVECTOR TransformNormal(const XMFLOAT3& n, const XMMATRIX& M)
    {
        return XMVector3TransformNormal(XMLoadFloat3(&n), M);
    }


    DirectX::XMFLOAT3 TransformPoint(
        const DirectX::XMFLOAT4X4& M,
        const DirectX::XMFLOAT3& v)
    {
        using namespace DirectX;

        XMMATRIX mat = XMLoadFloat4x4(&M);
        XMVECTOR p = XMVector3Transform(XMLoadFloat3(&v), mat);
        XMFLOAT3 out;
        XMStoreFloat3(&out, p);
        return out;
    }

    // ------------------------------------------------------------




    // ------------------------------------------------------------
    DirectX::XMFLOAT4X4 EvaluateNodeWorldRecursive(
        const ufbx_node* node,
        const ufbx_anim* anim,
        double           t,
        std::unordered_map<const ufbx_node*, DirectX::XMFLOAT4X4>& cache)
    {
        using namespace DirectX;


        std::unordered_map<const ufbx_node*, XMFLOAT4X4>::iterator it =
            cache.find(node);
        if (it != cache.end()) {
            return it->second;
        }


        ufbx_transform xf = ufbx_evaluate_transform(anim, node, t);
        ufbx_matrix    lm = ufbx_transform_to_matrix(&xf);
        XMFLOAT4X4     xm_local = ToXMMatrix(lm);
        XMMATRIX       L = XMLoadFloat4x4(&xm_local);

        XMMATRIX W;

        if (node->parent) {

            XMFLOAT4X4 parent_world =
                EvaluateNodeWorldRecursive(node->parent, anim, t, cache);
            XMMATRIX PW = XMLoadFloat4x4(&parent_world);

            W = L * PW;
        }
        else {

            W = L;
        }

        XMFLOAT4X4 xm_world;
        XMStoreFloat4x4(&xm_world, W);


        cache.insert(std::make_pair(node, xm_world));
        return xm_world;
    }

    //const ufbx_texture* GetDiffuseTexture(const ufbx_material* mat)
    //{
    //    if (!mat) return nullptr;


    //    if (mat->pbr.base_color.texture)
    //        return mat->pbr.base_color.texture;


    //    if (mat->fbx.diffuse_color.texture)
    //        return mat->fbx.diffuse_color.texture;

    //    if (mat->fbx.ambient_color.texture)
    //        return mat->fbx.ambient_color.texture;

    //    return nullptr;
    //}

} // namespace UfbxUtil
