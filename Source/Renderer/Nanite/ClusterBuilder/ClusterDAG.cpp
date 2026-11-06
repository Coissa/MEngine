#include "Cluster.h"

#include <Core/Containers/DisjointSet.h>

#include <algorithm>
#include <bit>
#include <unordered_map>

namespace GE::Render::Nanite
{
    namespace
    {
        using EdgeKey = std::uint64_t;

        struct PositionKey final
        {
            std::uint32_t x;
            std::uint32_t y;
            std::uint32_t z;

            bool operator==(const PositionKey&) const noexcept = default;
        };

        struct PositionKeyHash final
        {
            [[nodiscard]] std::size_t operator()(const PositionKey& key) const noexcept
            {
                std::uint64_t hash = 1469598103934665603ULL;
                hash = (hash ^ key.x) * 1099511628211ULL;
                hash = (hash ^ key.y) * 1099511628211ULL;
                hash = (hash ^ key.z) * 1099511628211ULL;
                return static_cast<std::size_t>(hash);
            }
        };

        [[nodiscard]] std::uint32_t PositionBits(float value) noexcept
        {
            return value == 0.0F ? 0U : std::bit_cast<std::uint32_t>(value);
        }

        [[nodiscard]] PositionKey MakePositionKey(Math::Vec3 position) noexcept
        {
            return {PositionBits(position[0]), PositionBits(position[1]), PositionBits(position[2])};
        }

        [[nodiscard]] std::vector<std::uint32_t> BuildGeometryVertexIds(
            std::span<const AssetFormats::StaticMeshVertex> vertices)
        {
            std::unordered_map<PositionKey, std::uint32_t, PositionKeyHash> position_ids;
            position_ids.reserve(vertices.size());

            std::vector<std::uint32_t> geometry_vertex_ids(vertices.size());
            for (std::uint32_t vertex = 0; vertex < vertices.size(); ++vertex)
            {
                const auto entry = position_ids.try_emplace(MakePositionKey(vertices[vertex].position),
                                                            static_cast<std::uint32_t>(position_ids.size()));
                geometry_vertex_ids[vertex] = entry.first->second;
            }
            return geometry_vertex_ids;
        }

        [[nodiscard]] constexpr EdgeKey MakeEdgeKey(std::uint32_t vertex0, std::uint32_t vertex1) noexcept
        {
            return (static_cast<std::uint64_t>(vertex0) << 32U) | static_cast<std::uint64_t>(vertex1);
        }

        [[nodiscard]] bool GroupsMatch(std::span<const std::uint32_t> triangle_groups, std::uint32_t left_triangle,
                                       std::uint32_t right_triangle) noexcept
        {
            return triangle_groups.empty() || triangle_groups[left_triangle] == triangle_groups[right_triangle];
        }
    } // namespace

    TriangleTopology BuildTriangleTopology(const TriangleTopologyInput& input)
    {
        const std::uint32_t triangle_count = static_cast<std::uint32_t>(input.indices.size() / 3U);

        TriangleTopology topology;
        topology.adjacent_edges.resize(input.indices.size());
        topology.island_ids.resize(triangle_count);
        topology.triangle_centers.resize(triangle_count);

        const std::vector<std::uint32_t> geometry_vertex_ids = BuildGeometryVertexIds(input.vertices);

        // A directed half-edge identifies both endpoints. The stored half-edge
        // index also identifies its owner triangle.
        std::unordered_map<EdgeKey, std::vector<std::uint32_t>> edge_buckets;
        edge_buckets.reserve(input.indices.size());

        for (std::uint32_t triangle_index = 0; triangle_index < triangle_count; ++triangle_index)
        {
            const std::uint32_t first_edge = triangle_index * 3U;
            for (std::uint32_t local_edge = 0; local_edge < 3U; ++local_edge)
            {
                const std::uint32_t edge_index = first_edge + local_edge;
                const std::uint32_t next_edge = first_edge + ((local_edge + 1U) % 3U);
                edge_buckets[MakeEdgeKey(geometry_vertex_ids[input.indices[edge_index]],
                                         geometry_vertex_ids[input.indices[next_edge]])]
                    .push_back(edge_index);
            }

            const Math::Vec3& position0 = input.vertices[input.indices[first_edge]].position;
            const Math::Vec3& position1 = input.vertices[input.indices[first_edge + 1U]].position;
            const Math::Vec3& position2 = input.vertices[input.indices[first_edge + 2U]].position;
            topology.triangle_centers[triangle_index] = (position0 + position1 + position2) / 3.0F;
        }

        Core::DisjointSet islands(triangle_count);
        for (std::uint32_t edge_index = 0; edge_index < input.indices.size(); ++edge_index)
        {
            const std::uint32_t triangle_index = edge_index / 3U;
            const std::uint32_t first_edge = triangle_index * 3U;
            const std::uint32_t local_edge = edge_index - first_edge;
            const std::uint32_t next_edge = first_edge + ((local_edge + 1U) % 3U);

            const EdgeKey reverse_key = MakeEdgeKey(geometry_vertex_ids[input.indices[next_edge]],
                                                    geometry_vertex_ids[input.indices[edge_index]]);
            const auto bucket = edge_buckets.find(reverse_key);
            if (bucket == edge_buckets.end())
            {
                continue;
            }

            std::vector<std::uint32_t>& adjacent = topology.adjacent_edges[edge_index];
            for (const std::uint32_t opposite_edge : bucket->second)
            {
                const std::uint32_t opposite_triangle = opposite_edge / 3U;
                if (opposite_triangle == triangle_index ||
                    !GroupsMatch(input.triangle_groups, triangle_index, opposite_triangle))
                {
                    continue;
                }

                adjacent.push_back(opposite_edge);
                static_cast<void>(islands.Union(triangle_index, opposite_triangle));
            }

            std::sort(adjacent.begin(), adjacent.end());
            adjacent.erase(std::unique(adjacent.begin(), adjacent.end()), adjacent.end());
        }

        // Compact roots into deterministic island IDs in triangle order.
        std::unordered_map<std::size_t, std::uint32_t> root_to_island;
        root_to_island.reserve(triangle_count);
        for (std::uint32_t triangle_index = 0; triangle_index < triangle_count; ++triangle_index)
        {
            const std::size_t root = islands.Find(triangle_index);
            const auto [iterator, inserted] =
                root_to_island.emplace(root, static_cast<std::uint32_t>(root_to_island.size()));
            static_cast<void>(inserted);
            topology.island_ids[triangle_index] = iterator->second;
        }
        topology.island_count = static_cast<std::uint32_t>(root_to_island.size());

        return topology;
    }

    TriangleTopology BuildTriangleTopology(const AssetFormats::StaticMeshAsset& mesh)
    {
        const TriangleTopologyInput input{.vertices = mesh.vertices, .indices = mesh.indices};
        return BuildTriangleTopology(input);
    }

    void BuildClusterTraversalData(ClusterDAG& dag)
    {
        for (uint32_t level = 0; level + 1 < dag.levels.size(); ++level)
        {
            const auto& current = dag.levels[level];
            const auto& next = dag.levels[level + 1];

            for (uint32_t i = 0; i < current.group_count; ++i)
            {
                const uint32_t group_id = current.group_begin + i;
                const auto& group = dag.groups[group_id];

                for (uint32_t p = 0; p < group.parent_count; ++p)
                {
                    dag.clusters[next.cluster_begin + group.parent_begin + p].generating_group_index = group_id;
                }

                for (const uint32_t child : group.children)
                {
                    dag.clusters[current.cluster_begin + child].group_index = group_id;
                }
            }
        }
    }

} // namespace GE::Render::Nanite
