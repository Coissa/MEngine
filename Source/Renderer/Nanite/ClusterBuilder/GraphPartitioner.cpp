#include "GraphPartitioner.h"
#include "Cluster.h"
#include "MortonCode.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <numeric>
#include <queue>
#include <vector>

namespace GE::Render::Nanite
{
    namespace
    {
        constexpr std::uint32_t TopologyEdgeCost = 4U * 65U;
        constexpr std::uint32_t LocalityEdgeCost = 1;
        constexpr std::uint32_t MaxLocalityLinks = 5;
        constexpr std::uint32_t LocalitySearchDistance = 16;
        constexpr std::uint32_t InvalidTriangle = (std::numeric_limits<std::uint32_t>::max)();

        struct GraphEdge final
        {
            std::uint32_t triangle;
            std::uint32_t cost;
        };

        using TriangleGraph = std::vector<std::vector<GraphEdge>>;

        void AddGraphEdge(TriangleGraph& graph, std::uint32_t from, std::uint32_t to, std::uint32_t cost,
                          bool accumulate)
        {
            for (GraphEdge& edge : graph[from])
            {
                if (edge.triangle == to)
                {
                    edge.cost = accumulate ? edge.cost + cost : (std::max)(edge.cost, cost);
                    return;
                }
            }
            graph[from].push_back({to, cost});
        }

        [[nodiscard]] Math::Vec3 NormalizeCenter(Math::Vec3 center, Math::Vec3 bounds_min,
                                                 float inverse_extent) noexcept
        {
            return (center - bounds_min) * inverse_extent;
        }

        struct Candidate final
        {
            std::uint32_t score;
            std::uint32_t morton_position;
            std::uint32_t triangle;
        };

        struct CandidateLess final
        {
            [[nodiscard]] bool operator()(const Candidate& left, const Candidate& right) const noexcept
            {
                if (left.score != right.score)
                {
                    return left.score < right.score;
                }
                if (left.morton_position != right.morton_position)
                {
                    return left.morton_position > right.morton_position;
                }
                return left.triangle > right.triangle;
            }
        };
    } // namespace

    TrianglePartition PartitionTriangles(const TriangleTopology& topology,
                                         std::span<const std::uint32_t> material_indices)
    {
        const std::uint32_t triangle_count = static_cast<std::uint32_t>(topology.triangle_centers.size());

        TrianglePartition partition;
        partition.indexes.reserve(triangle_count);
        partition.sorted_to.resize(triangle_count);
        if (triangle_count == 0)
        {
            return partition;
        }

        Math::Vec3 bounds_min = topology.triangle_centers[0];
        Math::Vec3 bounds_max = bounds_min;
        for (const Math::Vec3 center : topology.triangle_centers)
        {
            for (std::uint32_t axis = 0; axis < 3; ++axis)
            {
                bounds_min[axis] = (std::min)(bounds_min[axis], center[axis]);
                bounds_max[axis] = (std::max)(bounds_max[axis], center[axis]);
            }
        }

        const Math::Vec3 bounds_extent = bounds_max - bounds_min;
        const float maximum_extent = (std::max)(bounds_extent[0], (std::max)(bounds_extent[1], bounds_extent[2]));
        const float inverse_extent = maximum_extent > 0.0F ? 1.0F / maximum_extent : 0.0F;

        std::vector<std::uint32_t> morton_codes(triangle_count);
        std::vector<std::uint32_t> morton_order(triangle_count);
        std::iota(morton_order.begin(), morton_order.end(), 0U);

        for (std::uint32_t triangle = 0; triangle < triangle_count; ++triangle)
        {
            morton_codes[triangle] =
                CalculateMortonCode(NormalizeCenter(topology.triangle_centers[triangle], bounds_min, inverse_extent));
        }

        std::sort(morton_order.begin(), morton_order.end(),
                  [&](std::uint32_t left, std::uint32_t right)
                  {
                      if (morton_codes[left] != morton_codes[right])
                      {
                          return morton_codes[left] < morton_codes[right];
                      }
                      return left < right;
                  });

        std::vector<std::uint32_t> morton_position(triangle_count);
        for (std::uint32_t position = 0; position < triangle_count; ++position)
        {
            morton_position[morton_order[position]] = position;
        }

        TriangleGraph graph(triangle_count);
        for (std::uint32_t triangle = 0; triangle < triangle_count; ++triangle)
        {
            const std::uint32_t first_edge = triangle * 3U;
            for (std::uint32_t local_edge = 0; local_edge < 3U; ++local_edge)
            {
                for (const std::uint32_t opposite_edge : topology.adjacent_edges[first_edge + local_edge])
                {
                    AddGraphEdge(graph, triangle, opposite_edge / 3U, TopologyEdgeCost, true);
                }
            }
        }

        std::vector<std::uint32_t> island_sizes(triangle_count);
        for (const std::uint32_t island : topology.island_ids)
        {
            ++island_sizes[island];
        }

        for (std::uint32_t position = 0; position < triangle_count; ++position)
        {
            const std::uint32_t triangle = morton_order[position];
            const std::uint32_t island = topology.island_ids[triangle];
            if (island_sizes[island] >= ClusterMaxTriangles)
            {
                continue;
            }

            std::array<std::uint32_t, MaxLocalityLinks> closest;
            std::array<float, MaxLocalityLinks> closest_distance;
            closest.fill(InvalidTriangle);
            closest_distance.fill((std::numeric_limits<float>::max)());

            for (const std::int32_t direction : {-1, 1})
            {
                for (std::uint32_t step = 1; step <= LocalitySearchDistance; ++step)
                {
                    const std::int64_t adjacent_position =
                        static_cast<std::int64_t>(position) +
                        static_cast<std::int64_t>(direction) * static_cast<std::int64_t>(step);
                    if (adjacent_position < 0 || adjacent_position >= triangle_count)
                    {
                        break;
                    }

                    std::uint32_t adjacent = morton_order[static_cast<std::uint32_t>(adjacent_position)];
                    if (topology.island_ids[adjacent] == island)
                    {
                        continue;
                    }
                    if (!material_indices.empty() && material_indices[adjacent] != material_indices[triangle])
                    {
                        continue;
                    }

                    float distance =
                        Math::LengthSquared(topology.triangle_centers[triangle] - topology.triangle_centers[adjacent]);

                    for (std::uint32_t link = 0; link < MaxLocalityLinks; ++link)
                    {
                        if (distance < closest_distance[link] ||
                            (distance == closest_distance[link] && adjacent < closest[link]))
                        {
                            std::swap(adjacent, closest[link]);
                            std::swap(distance, closest_distance[link]);
                        }
                    }
                }
            }

            for (const std::uint32_t adjacent : closest)
            {
                if (adjacent != InvalidTriangle)
                {
                    AddGraphEdge(graph, triangle, adjacent, LocalityEdgeCost, false);
                    AddGraphEdge(graph, adjacent, triangle, LocalityEdgeCost, false);
                }
            }
        }

        const std::uint32_t partition_count = (triangle_count + ClusterMaxTriangles - 1U) / ClusterMaxTriangles;
        const std::uint32_t base_partition_size = triangle_count / partition_count;
        const std::uint32_t larger_partition_count = triangle_count % partition_count;

        std::vector<bool> assigned(triangle_count, false);
        std::vector<std::uint32_t> scores(triangle_count);
        std::uint32_t next_seed_position = 0;

        for (std::uint32_t partition_index = 0; partition_index < partition_count; ++partition_index)
        {
            const std::uint32_t target_size =
                base_partition_size + (partition_index < larger_partition_count ? 1U : 0U);
            const std::uint32_t range_begin = static_cast<std::uint32_t>(partition.indexes.size());

            std::priority_queue<Candidate, std::vector<Candidate>, CandidateLess> candidates;
            std::fill(scores.begin(), scores.end(), 0U);

            while (assigned[morton_order[next_seed_position]])
            {
                ++next_seed_position;
            }

            const auto assign_triangle = [&](std::uint32_t triangle)
            {
                assigned[triangle] = true;
                partition.indexes.push_back(triangle);

                for (const GraphEdge& edge : graph[triangle])
                {
                    if (!assigned[edge.triangle])
                    {
                        scores[edge.triangle] += edge.cost;
                        candidates.push({scores[edge.triangle], morton_position[edge.triangle], edge.triangle});
                    }
                }
            };

            assign_triangle(morton_order[next_seed_position]);

            while (partition.indexes.size() - range_begin < target_size)
            {
                while (!candidates.empty() && (assigned[candidates.top().triangle] ||
                                               candidates.top().score != scores[candidates.top().triangle]))
                {
                    candidates.pop();
                }

                std::uint32_t next_triangle = InvalidTriangle;
                if (!candidates.empty())
                {
                    next_triangle = candidates.top().triangle;
                    candidates.pop();
                }
                else
                {
                    while (assigned[morton_order[next_seed_position]])
                    {
                        ++next_seed_position;
                    }
                    next_triangle = morton_order[next_seed_position];
                }

                assign_triangle(next_triangle);
            }

            partition.ranges.push_back({range_begin, static_cast<std::uint32_t>(partition.indexes.size())});
        }

        for (std::uint32_t position = 0; position < triangle_count; ++position)
        {
            partition.sorted_to[partition.indexes[position]] = position;
        }

        return partition;
    }
} // namespace GE::Render::Nanite
