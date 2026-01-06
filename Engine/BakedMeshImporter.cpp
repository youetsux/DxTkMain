#include "BakedMeshImporter.h"

#include "ufbx.h"

#include <cstring>
#include <algorithm>

namespace
{
    struct TempVertexKey
    {
        float px, py, pz;
        float nx, ny, nz;
        float u, v;
    };

    static TempVertexKey MakeKey(const ufbx_mesh* mesh, uint32_t vertex_index)
    {
        TempVertexKey k{};
        if (mesh->vertex_position.exists) {
            ufbx_vec3 p = ufbx_get_vertex_vec3(&mesh->vertex_position, (size_t)vertex_index);
            k.px = (float)p.x; k.py = (float)p.y; k.pz = (float)p.z;
        }
        if (mesh->vertex_normal.exists) {
            ufbx_vec3 n = ufbx_get_vertex_vec3(&mesh->vertex_normal, (size_t)vertex_index);
            k.nx = (float)n.x; k.ny = (float)n.y; k.nz = (float)n.z;
        }
        if (mesh->vertex_uv.exists) {
            ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, (size_t)vertex_index);
            k.u = (float)uv.x; k.v = (float)uv.y;
        }
        return k;
    }
}

bool BakedMeshImporter::ImportMesh(const ufbx_scene* /*scene*/, const ufbx_mesh* mesh, BakedMeshImportResult& out_result)
{
    if (!mesh) return false;

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

        uint32_t written = ufbx_triangulate_face(tri_indices.data(), tri_indices.size(), mesh, face);

        for (uint32_t ti = 0; ti + 2 < written; ti += 3)
        {
            for (uint32_t k = 0; k < 3; ++k)
            {
                uint32_t vix = tri_indices[ti + k];

                corner_keys.push_back(MakeKey(mesh, vix));
                corner_corner.push_back(vix);

                if (mesh->vertex_indices.count > 0 && vix < (uint32_t)mesh->vertex_indices.count) {
                    corner_cp.push_back(mesh->vertex_indices.data[vix]);
                }
                else {
                    corner_cp.push_back(0);
                }
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

    std::vector<int32_t> first((size_t)unique_vertices, -1);
    for (size_t i = 0; i < indices.size(); ++i) {
        uint32_t ui = indices[i];
        if (ui < unique_vertices && first[ui] < 0) first[ui] = (int32_t)i;
    }

    std::vector<TempVertexKey> unique_keys(unique_vertices);
    out_result.fbx_control_point_index_of_runtime.resize(unique_vertices);
    out_result.fbx_corner_index_of_runtime.resize(unique_vertices);

    for (size_t ui = 0; ui < unique_vertices; ++ui)
    {
        int32_t src = first[ui];
        if (src < 0) src = 0;

        unique_keys[ui] = corner_keys[(size_t)src];
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

    return true;
}
