#pragma once

#include "ClusterBuilder/Cluster.h"

#include <cstdint>
#include <span>
#include <vector>

namespace GE::Render::Nanite
{
    struct ClusterDesc
    {
        uint32_t vertex_begin;
        uint32_t vertex_count;
        uint32_t index_begin;
        uint32_t index_count;

        uint32_t generating_group_index;
        uint32_t group_index;
        uint32_t node_index;

        Math::Vec3 bounds_min;
        Math::Vec3 bounds_max;

        Math::Vec3 lod_center;
        float lod_r;
        float lod_error;
    };

    struct GroupDesc
    {
        uint32_t children_begin;
        uint32_t children_count;

        uint32_t parent_begin;
        uint32_t parent_count;

        Math::Vec3 lod_center;
        float lod_r;
        float parent_lod_error;
    };

    struct NodeDesc
    {
        uint32_t node_index;
        uint32_t root_begin;
        uint32_t root_count;
    };

    struct NaniteMeshData
    {
        std::vector<AssetFormats::StaticMeshVertex> vertices;
        std::vector<uint16_t> indices;

        std::vector<ClusterDesc> clusters;
        std::vector<GroupDesc> groups;
        std::vector<uint32_t> children;
        std::vector<NodeDesc> nodes;
    };

    [[nodiscard]] NaniteMeshData PackClusterDAG(std::span<const ClusterDAG> dags);

} // namespace GE::Render::Nanite
