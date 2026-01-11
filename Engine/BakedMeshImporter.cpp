#define NOMINMAX

#include "BakedMeshImporter.h"

#include "ufbx.h"

#include "UfbxUtil.h"
#include <Windows.h>

#include <cstring>
#include <algorithm>
#include <unordered_map>

namespace
{
    struct TempVertexKey
    {
        float px, py, pz;
        float nx, ny, nz;
        float u, v;
    };

    TempVertexKey MakeKey(const ufbx_mesh* mesh, uint32_t index, const ufbx_vertex_vec2* uv_stream, const ufbx_matrix* uv_to_texture)
    {
        TempVertexKey k{};
        if (mesh->vertex_position.exists) {
            ufbx_vec3 p = ufbx_get_vertex_vec3(&mesh->vertex_position, (size_t)index);
            k.px = (float)p.x; k.py = (float)p.y; k.pz = (float)p.z;
        }
        if (mesh->vertex_normal.exists) {
            ufbx_vec3 n = ufbx_get_vertex_vec3(&mesh->vertex_normal, (size_t)index);
            k.nx = (float)n.x; k.ny = (float)n.y; k.nz = (float)n.z;
        }        if (uv_stream && uv_stream->exists) {
            ufbx_vec2 uv = ufbx_get_vertex_vec2(uv_stream, (size_t)index);
            if (uv_to_texture) {
                ufbx_vec3 uvw = { (ufbx_real)uv.x, (ufbx_real)uv.y, (ufbx_real)0.0 };
                ufbx_vec3 tuv = ufbx_transform_position(uv_to_texture, uvw);
                k.u = (float)tuv.x;
                k.v = (float)tuv.y;
            } else {
                k.u = (float)uv.x;
                k.v = (float)uv.y;
            }

            // FBX/DCC UV convention (V up) -> DirectX convention (V down)
            k.v = 1.0f - k.v;
        }
        return k;
    }
    struct Influence
    {
        std::uint16_t bone = 0;
        float weight = 0.0f;
    };

    std::uint16_t GetOrAddBoneIndex(std::unordered_map<std::uint32_t, std::uint16_t>& map,
                                    std::vector<std::uint32_t>& element_ids,
                                    std::uint32_t element_id)
    {
        auto it = map.find(element_id);
        if (it != map.end()) return it->second;

        const std::size_t next = element_ids.size();
        if (next >= 0xFFFFu) return 0; // clamp, extremely unlikely

        std::uint16_t ix = (std::uint16_t)next;
        element_ids.push_back(element_id);
        map.emplace(element_id, ix);
        return ix;
    }

    void PackTop4Normalized(const std::vector<Influence>& in, BakedMeshImportResult::BakedVertexSkin& out)
    {
        // Initialize to zero
        for (int i = 0; i < 4; ++i) { out.bone[i] = 0; out.weight[i] = 0.0f; }
        if (in.empty()) return;

        std::vector<Influence> tmp = in;
        std::sort(tmp.begin(), tmp.end(), [](const Influence& a, const Influence& b) { return a.weight > b.weight; });

        float sum = 0.0f;
        const std::size_t n = std::min<std::size_t>(4, tmp.size());
        for (std::size_t i = 0; i < n; ++i) sum += tmp[i].weight;
        if (sum <= 0.0f) return;

        for (std::size_t i = 0; i < n; ++i) {
            out.bone[i] = tmp[i].bone;
            out.weight[i] = tmp[i].weight / sum;
        }
    }

    void BakeSkinInfluences(const ufbx_mesh* mesh, BakedMeshImportResult& out_result)
    {
        out_result.skin_bone_node_element_ids.clear();
        out_result.skin.clear();

        if (!mesh) return;
        if (mesh->skin_deformers.count == 0) return;
        if (out_result.vertex_count == 0) return;
        if (mesh->num_vertices == 0) return;

        out_result.skin.resize((std::size_t)out_result.vertex_count);

        std::vector<std::vector<Influence>> cp_influences;
        cp_influences.resize(mesh->num_vertices);

        std::unordered_map<std::uint32_t, std::uint16_t> bone_map;
        bone_map.reserve(128);

        for (size_t di = 0; di < mesh->skin_deformers.count; ++di) {
            const ufbx_skin_deformer* def = mesh->skin_deformers.data[di];
            if (!def) continue;

            const size_t cp_count = std::min(mesh->num_vertices, def->vertices.count);
            for (size_t cp = 0; cp < cp_count; ++cp) {
                const ufbx_skin_vertex sv = def->vertices.data[cp];

                const size_t wb = (size_t)sv.weight_begin;
                const size_t we = std::min(wb + (size_t)sv.num_weights, def->weights.count);
                for (size_t wi = wb; wi < we; ++wi) {
                    const ufbx_skin_weight sw = def->weights.data[wi];
                    if (sw.weight <= 0.0) continue;
                    if (sw.cluster_index >= def->clusters.count) continue;

                    const ufbx_skin_cluster* cl = def->clusters.data[sw.cluster_index];
                    if (!cl || !cl->bone_node) continue;

                    const std::uint32_t bone_element_id = cl->bone_node->element_id;
                    const std::uint16_t bone_ix = GetOrAddBoneIndex(bone_map, out_result.skin_bone_node_element_ids, bone_element_id);
                    cp_influences[cp].push_back(Influence{ bone_ix, (float)sw.weight });
                }
            }
        }

        for (std::size_t ui = 0; ui < out_result.skin.size(); ++ui) {
            if (ui >= out_result.fbx_control_point_index_of_runtime.size()) break;
            const std::uint32_t cp = out_result.fbx_control_point_index_of_runtime[ui];
            if (cp >= cp_influences.size()) continue;
            PackTop4Normalized(cp_influences[cp], out_result.skin[ui]);
        }
    }

}

bool BakedMeshImporter::ImportMesh(const ufbx_scene* scene, const ufbx_mesh* mesh, BakedMeshImportResult& out_result)
{
    if (!mesh) return false;

    const ufbx_material* mat = nullptr;
    const ufbx_texture* tex = nullptr;
    if (scene && mesh->materials.count > 0) {
        mat = mesh->materials.data[0];
        tex = UfbxUtil::GetDiffuseTexture(mat);
    }

    uint32_t uv_set_index = 0;
    if (tex && tex->uv_set.length > 0 && tex->uv_set.data && mesh->uv_sets.count > 0) {
        for (size_t i = 0; i < mesh->uv_sets.count; ++i) {
            const ufbx_uv_set& us = mesh->uv_sets.data[i];
            if (us.name.length == tex->uv_set.length && us.name.data && tex->uv_set.data) {
                if (std::memcmp(us.name.data, tex->uv_set.data, tex->uv_set.length) == 0) {
                    uv_set_index = (uint32_t)i;
                    break;
                }
            }
        }
    }

    const ufbx_vertex_vec2* uv_stream = nullptr;
    if (mesh->uv_sets.count > 0 && uv_set_index < (uint32_t)mesh->uv_sets.count) {
        uv_stream = &mesh->uv_sets.data[uv_set_index].vertex_uv;
    } else if (mesh->vertex_uv.exists) {
        uv_stream = &mesh->vertex_uv;
    }

    
    // ---- UV/TEXTURE debug (no behavior change) ----
    if (tex) {
        char buf[512];
        sprintf_s(buf,
            "BakedMeshImporter: tex='%s' uv_set='%s' has_uv_transform=%d wrap_u=%d wrap_v=%d uv_sets=%u selected_uv=%u\n",
            tex->filename.data ? tex->filename.data : "(nofile)",
            tex->uv_set.data ? tex->uv_set.data : "(none)",
            tex->has_uv_transform ? 1 : 0,
            (int)tex->wrap_u, (int)tex->wrap_v,
            (unsigned)mesh->uv_sets.count, (unsigned)uv_set_index);
        OutputDebugStringA(buf);

        if (tex->has_uv_transform) {
            const ufbx_matrix* mt = &tex->uv_to_texture;
            sprintf_s(buf,
                "  uv_to_texture: [%g %g %g %g] [%g %g %g %g] [%g %g %g %g]\n",
                (double)mt->m00, (double)mt->m01, (double)mt->m02, (double)mt->m03,
                (double)mt->m10, (double)mt->m11, (double)mt->m12, (double)mt->m13,
                (double)mt->m20, (double)mt->m21, (double)mt->m22, (double)mt->m23);
            OutputDebugStringA(buf);
        }
    }
    // ----------------------------------------------
const ufbx_matrix* uv_to_texture = nullptr;
    if (tex && tex->has_uv_transform) {
        uv_to_texture = &tex->uv_to_texture;
    }

    out_result = BakedMeshImportResult{};
    if (mesh->name.length > 0 && mesh->name.data) {
        out_result.name.assign(mesh->name.data, mesh->name.length);
    }

    std::vector<TempVertexKey> corner_keys;
    std::vector<uint32_t> corner_cp;
    std::vector<uint32_t> corner_corner;

    corner_keys.reserve(mesh->num_indices);
    corner_cp.reserve(mesh->num_indices);
    corner_corner.reserve(mesh->num_indices);

    std::vector<uint32_t> tri_indices;

    for (size_t fi = 0; fi < mesh->faces.count; ++fi)
    {
        ufbx_face face = mesh->faces.data[fi];
        if (face.num_indices < 3) continue;

        size_t max_tri_indices = (size_t)(face.num_indices - 2) * 3;
        tri_indices.resize(max_tri_indices);

        uint32_t num_tris = ufbx_triangulate_face(tri_indices.data(), tri_indices.size(), mesh, face);

        for (uint32_t tri = 0; tri < num_tris; ++tri)
        {
            uint32_t base = tri * 3;
            for (uint32_t k = 0; k < 3; ++k)
            {
                // NOTE:
                // ufbx_triangulate_face() outputs indices that refer to the mesh index buffer ("corner" indices).
                // `ufbx_triangulate_face()` outputs indices in the mesh "index" domain
                // (0..mesh->num_indices-1). Use those directly to fetch split attributes.
                const uint32_t index_ix = tri_indices[(size_t)base + (size_t)k];
                corner_keys.push_back(MakeKey(mesh, index_ix, uv_stream, uv_to_texture));

                // Logical vertex index (control point) corresponding to this split index.
                uint32_t cp_ix = 0;
                if (mesh->vertex_indices.count > 0 && index_ix < (uint32_t)mesh->vertex_indices.count) {
                    cp_ix = mesh->vertex_indices.data[index_ix];
                }
                corner_cp.push_back(cp_ix);
                corner_corner.push_back(index_ix);
            }
        }
    }

    if (corner_keys.empty()) return false;

    std::vector<uint32_t> indices(corner_keys.size(), 0);

    ufbx_vertex_stream stream{};
    stream.data = corner_keys.data();
    stream.vertex_count = corner_keys.size();
    stream.vertex_size = sizeof(TempVertexKey);

    ufbx_error error{};
    size_t unique_vertices = ufbx_generate_indices(&stream, 1, indices.data(), indices.size(), nullptr, &error);
    if (unique_vertices == 0) return false;


    // `ufbx_generate_indices()` compacts the vertex stream in-place so the
    // deduplicated vertices are in `corner_keys[0..unique_vertices)`. The `indices`
    // array maps old corner vertex -> new unique vertex.
    std::vector<int32_t> rep((size_t)unique_vertices, -1);
    for (size_t ci = 0; ci < indices.size(); ++ci)
    {
        uint32_t ui = indices[ci];
        if (ui < unique_vertices && rep[ui] < 0) rep[ui] = (int32_t)ci;
    }

    std::vector<TempVertexKey> unique_keys(unique_vertices);
    out_result.fbx_control_point_index_of_runtime.resize(unique_vertices);
    out_result.fbx_corner_index_of_runtime.resize(unique_vertices);

    for (size_t ui = 0; ui < unique_vertices; ++ui)
    {
        unique_keys[ui] = corner_keys[ui];

        int32_t src = rep[ui];
        if (src < 0) src = 0;
        out_result.fbx_corner_index_of_runtime[ui] = corner_corner[(size_t)src];
        out_result.fbx_control_point_index_of_runtime[ui] = corner_cp[(size_t)src];
    }

    out_result.vertex_stride = (uint32_t)sizeof(TempVertexKey);
    out_result.vertex_count = (uint32_t)unique_vertices;
    out_result.vertex_bytes.resize(sizeof(TempVertexKey) * unique_vertices);
    std::memcpy(out_result.vertex_bytes.data(), unique_keys.data(), sizeof(TempVertexKey) * unique_vertices);

    out_result.indices = std::move(indices);

    BakedMeshImportResult::Submesh sm{};
    sm.index_start = 0;
    sm.index_count = (uint32_t)out_result.indices.size();
    sm.material_index = 0;
    out_result.submeshes.push_back(sm);

    BakeSkinInfluences(mesh, out_result);

    return true;
}
