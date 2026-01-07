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

    static TempVertexKey MakeKey(const ufbx_mesh* mesh, uint32_t index)
    {
        TempVertexKey k{};
        if (mesh->vertex_position.exists) {
            ufbx_vec3 p = ufbx_get_vertex_vec3(&mesh->vertex_position, (size_t)index);
            k.px = (float)p.x; k.py = (float)p.y; k.pz = (float)p.z;
        }
        if (mesh->vertex_normal.exists) {
            ufbx_vec3 n = ufbx_get_vertex_vec3(&mesh->vertex_normal, (size_t)index);
            k.nx = (float)n.x; k.ny = (float)n.y; k.nz = (float)n.z;
        }
        if (mesh->vertex_uv.exists) {
            ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, (size_t)index);
            k.u = (float)uv.x; k.v = 1.0f - (float)uv.y;
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
                corner_keys.push_back(MakeKey(mesh, index_ix));

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

    return true;
}
