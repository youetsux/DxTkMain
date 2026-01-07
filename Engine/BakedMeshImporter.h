#pragma once
#include <cstdint>
#include <vector>
#include <string>

struct ufbx_scene;
struct ufbx_mesh;

struct BakedMeshImportResult
{
    struct Submesh
    {
        uint32_t index_start = 0;
        uint32_t index_count = 0;
        uint32_t material_index = 0;
    };

    std::string name;

    std::vector<std::uint8_t> vertex_bytes;
    uint32_t vertex_stride = 0;
    uint32_t vertex_count = 0;

    std::vector<std::uint32_t> indices;
    std::vector<Submesh> submeshes;

    std::vector<std::uint32_t> fbx_control_point_index_of_runtime;
    std::vector<std::uint32_t> fbx_corner_index_of_runtime;

    bool IsValid() const
    {
        if (vertex_stride == 0) return false;
        if (vertex_count == 0) return false;
        if (vertex_bytes.size() != std::size_t(vertex_stride) * std::size_t(vertex_count)) return false;
        return true;
    }
};

class BakedMeshImporter
{
public:
    static bool ImportMesh(const ufbx_scene* scene, const ufbx_mesh* mesh, BakedMeshImportResult& out_result);
};
