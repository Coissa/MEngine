#pragma once

#include <Assets/Formats/StaticAssetFormats.h>
#include <Core/Math/vector.h>

#include "ClusterDAG.h"
#include "GraphPartitioner.h"

#include <span>
#include <cstdint>
#include <vector>

namespace GE::Render::Nanite
{
    inline constexpr std::uint32_t ClusterMaxTriangles = 128;
    inline constexpr std::uint32_t ClusterMaxVertices = 256;
    inline constexpr std::uint32_t ClusterGroupMaxSize = 32;

    struct Cluster final
    {
        std::vector<AssetFormats::StaticMeshVertex> vertices;
        std::vector<std::uint8_t> indices;
        std::vector<std::uint32_t> material_indices;
        std::vector<std::uint8_t> external_edges;
        Math::Vec3 bounds_min{};
        Math::Vec3 bounds_max{};
        std::uint32_t generating_group_index = UINT32_MAX;
        std::uint32_t group_index = UINT32_MAX;
        Math::Vec3 lod_center{};
        float lod_r = 0.0F;
        float lod_error = 0.0F;
    };

    struct LeafClusterTriangles
    {
        std::vector<std::uint32_t> triangles;
    };

    struct ClusterGraphEdge final
    {
        std::uint32_t cluster;
        std::uint32_t shared_edge_count;
    };

    struct ClusterHalfEdge final
    {
        std::uint32_t cluster;
        std::uint32_t edge;
    };

    using ClusterGraph = std::vector<std::vector<ClusterGraphEdge>>;

    struct ClusterGroup final
    {
        Math::Vec3 lod_center{};
        std::vector<std::uint32_t> children;
        std::uint32_t parent_begin = 0;
        std::uint32_t parent_count = 0;
        float lod_r = 0;
        float parent_lod_error = 0.0F;
    };

    struct MergedCluster final
    {
        std::vector<AssetFormats::StaticMeshVertex> vertices;
        std::vector<std::uint32_t> indices;
        std::vector<std::uint32_t> material_indices;
        std::vector<std::uint8_t> external_edges;
        Math::Vec3 bounds_min{};
        Math::Vec3 bounds_max{};
    };

    struct NodeClusterLevel final
    {
        std::uint32_t node_index = 0;
        std::vector<Cluster> clusters;
        std::vector<ClusterGroup> groups;
        std::vector<MergedCluster> merged_groups;
        std::vector<Cluster> parent_clusters;
    };

    struct LevelRange final
    {
        std::uint32_t group_begin = 0;
        std::uint32_t group_count = 0;
        std::uint32_t cluster_begin = 0;
        std::uint32_t cluster_count = 0;
    };

    struct ClusterDAG final
    {
        std::uint32_t node_index = 0;
        std::vector<LevelRange> levels;
        std::vector<ClusterGroup> groups;
        std::vector<Cluster> clusters;
    };

    [[nodiscard]] ClusterGraph BuildClusterGraph(std::span<const Cluster> clusters);

    [[nodiscard]] std::vector<ClusterGroup> GroupClusters(std::span<const Cluster> clusters, const ClusterGraph& graph);

    [[nodiscard]] MergedCluster MergeClusterGroup(std::span<const Cluster> clusters, const ClusterGroup& group);

    // Simplifies the merged geometry in place and returns its maximum QEM error.
    [[nodiscard]] float SimplifyMergedCluster(MergedCluster& merged, std::uint32_t target_triangle_count);

    [[nodiscard]] std::vector<Cluster> BuildParentClusters(std::span<const Cluster> clusters,
                                                           std::span<ClusterGroup> groups,
                                                           std::span<const MergedCluster> merged_groups);

    [[nodiscard]] std::vector<NodeClusterLevel> BuildLeafClusterLevels(const AssetFormats::StaticMeshAsset& mesh,
                                                                       bool build_parents = true);

    [[nodiscard]] std::vector<std::uint32_t> BuildTriangleMaterialIndices(const AssetFormats::StaticMeshAsset& mesh);

    [[nodiscard]] std::vector<Cluster> BuildLeafClusters(const AssetFormats::StaticMeshAsset& mesh,
                                                         const TriangleTopology& topology,
                                                         const TrianglePartition& partition,
                                                         std::span<const std::uint32_t> triangle_material_indices,
                                                         std::span<const std::uint8_t> source_external_edges = {});

    void BuildClusterTraversalData(ClusterDAG& dag);

    [[nodiscard]] std::vector<ClusterDAG> ReduceMesh(const AssetFormats::StaticMeshAsset& mesh);
} // namespace GE::Render::Nanite
