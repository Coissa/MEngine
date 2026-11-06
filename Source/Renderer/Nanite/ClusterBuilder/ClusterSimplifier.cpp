#include "Cluster.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <map>

namespace GE::Render::Nanite
{
    namespace
    {
        using EdgeKey = std::uint64_t;

        struct Triangle final
        {
            std::array<std::uint32_t, 3> vertices;
            std::array<std::uint8_t, 3> external_edges;
            std::uint32_t material;
        };

        struct Quadric final
        {
            // xx, xy, xz, xw, yy, yz, yw, zz, zw, ww
            std::array<double, 10> values{};

            void AddPlane(Math::Vec3 normal, double distance)
            {
                const std::array<double, 4> plane{normal[0], normal[1], normal[2], distance};
                std::uint32_t index = 0;
                for (std::uint32_t row = 0; row < 4; ++row)
                {
                    for (std::uint32_t column = row; column < 4; ++column)
                    {
                        values[index++] += plane[row] * plane[column];
                    }
                }
            }

            void Add(const Quadric& other)
            {
                for (std::uint32_t index = 0; index < values.size(); ++index)
                {
                    values[index] += other.values[index];
                }
            }

            [[nodiscard]] double Evaluate(Math::Vec3 position) const
            {
                const double x = position[0];
                const double y = position[1];
                const double z = position[2];
                return values[0] * x * x + 2.0 * values[1] * x * y + 2.0 * values[2] * x * z + 2.0 * values[3] * x +
                       values[4] * y * y + 2.0 * values[5] * y * z + 2.0 * values[6] * y + values[7] * z * z +
                       2.0 * values[8] * z + values[9];
            }
        };

        struct EdgeUse final
        {
            std::uint32_t triangle;
            std::uint32_t local_edge;
        };

        struct EdgeInfo final
        {
            std::array<EdgeUse, 2> uses{};
            std::uint32_t count = 0;
        };

        struct Candidate final
        {
            std::uint32_t first;
            std::uint32_t second;
            Math::Vec3 position;
            double error;
            float edge_length_squared;
            std::uint32_t placement;
        };

        [[nodiscard]] EdgeKey MakeEdgeKey(std::uint32_t first, std::uint32_t second)
        {
            if (first > second)
            {
                std::swap(first, second);
            }
            return (static_cast<EdgeKey>(first) << 32U) | second;
        }

        [[nodiscard]] std::array<std::uint32_t, 8> VertexKey(const AssetFormats::StaticMeshVertex& vertex)
        {
            const auto bits = [](float value) { return std::bit_cast<std::uint32_t>(value == 0.0F ? 0.0F : value); };
            return {bits(vertex.position[0]), bits(vertex.position[1]), bits(vertex.position[2]),
                    bits(vertex.normal[0]),   bits(vertex.normal[1]),   bits(vertex.normal[2]),
                    bits(vertex.texcoord[0]), bits(vertex.texcoord[1])};
        }

        [[nodiscard]] std::map<EdgeKey, EdgeInfo> BuildEdges(const std::vector<Triangle>& triangles)
        {
            std::map<EdgeKey, EdgeInfo> edges;
            for (std::uint32_t triangle_index = 0; triangle_index < triangles.size(); ++triangle_index)
            {
                const Triangle& triangle = triangles[triangle_index];
                for (std::uint32_t edge = 0; edge < 3; ++edge)
                {
                    EdgeInfo& info = edges[MakeEdgeKey(triangle.vertices[edge], triangle.vertices[(edge + 1U) % 3U])];
                    if (info.count < info.uses.size())
                    {
                        info.uses[info.count] = {triangle_index, edge};
                    }
                    ++info.count;
                }
            }
            return edges;
        }

        [[nodiscard]] bool CanCollapse(const Candidate& candidate, const std::vector<Triangle>& triangles,
                                       const std::vector<AssetFormats::StaticMeshVertex>& vertices,
                                       const std::map<EdgeKey, EdgeInfo>& edges)
        {
            const EdgeInfo& collapsed_edge = edges.at(MakeEdgeKey(candidate.first, candidate.second));
            const std::uint32_t opposite0 =
                triangles[collapsed_edge.uses[0].triangle].vertices[(collapsed_edge.uses[0].local_edge + 2U) % 3U];
            const std::uint32_t opposite1 =
                triangles[collapsed_edge.uses[1].triangle].vertices[(collapsed_edge.uses[1].local_edge + 2U) % 3U];
            if (opposite0 == opposite1)
            {
                return false;
            }

            // The two endpoints may share only the two vertices opposite this edge.
            for (const auto& [key, info] : edges)
            {
                static_cast<void>(info);
                const std::uint32_t first = static_cast<std::uint32_t>(key >> 32U);
                const std::uint32_t second = static_cast<std::uint32_t>(key);
                const bool first_neighbour = first == candidate.first || second == candidate.first;
                if (!first_neighbour)
                {
                    continue;
                }
                const std::uint32_t neighbour = first == candidate.first ? second : first;
                if (neighbour == candidate.second || neighbour == opposite0 || neighbour == opposite1)
                {
                    continue;
                }
                const EdgeKey other_edge = MakeEdgeKey(candidate.second, neighbour);
                if (edges.contains(other_edge))
                {
                    return false;
                }
            }

            for (const Triangle& triangle : triangles)
            {
                const bool has_first = std::ranges::find(triangle.vertices, candidate.first) != triangle.vertices.end();
                const bool has_second =
                    std::ranges::find(triangle.vertices, candidate.second) != triangle.vertices.end();
                if (!has_first && !has_second)
                {
                    continue;
                }
                if (has_first && has_second)
                {
                    continue;
                }

                std::array<Math::Vec3, 3> before;
                std::array<Math::Vec3, 3> after;
                for (std::uint32_t corner = 0; corner < 3; ++corner)
                {
                    before[corner] = vertices[triangle.vertices[corner]].position;
                    after[corner] =
                        triangle.vertices[corner] == candidate.first || triangle.vertices[corner] == candidate.second
                            ? candidate.position
                            : before[corner];
                }
                const Math::Vec3 old_normal = Math::Cross(before[1] - before[0], before[2] - before[0]);
                const Math::Vec3 new_normal = Math::Cross(after[1] - after[0], after[2] - after[0]);
                if (Math::Dot(old_normal, new_normal) <= 0.0F)
                {
                    return false;
                }
            }
            return true;
        }
    } // namespace

    float SimplifyMergedCluster(MergedCluster& merged, std::uint32_t target_triangle_count)
    {
        if (merged.indices.size() / 3U <= target_triangle_count)
        {
            return 0.0F;
        }

        std::map<std::array<std::uint32_t, 8>, std::uint32_t> vertex_ids;
        std::vector<AssetFormats::StaticMeshVertex> vertices;
        std::vector<std::uint32_t> canonical_ids(merged.vertices.size());
        for (std::uint32_t index = 0; index < merged.vertices.size(); ++index)
        {
            const auto [entry, inserted] =
                vertex_ids.try_emplace(VertexKey(merged.vertices[index]), static_cast<std::uint32_t>(vertices.size()));
            if (inserted)
            {
                vertices.push_back(merged.vertices[index]);
            }
            canonical_ids[index] = entry->second;
        }

        std::vector<Triangle> triangles;
        triangles.reserve(merged.indices.size() / 3U);
        for (std::uint32_t triangle = 0; triangle < merged.indices.size() / 3U; ++triangle)
        {
            const std::uint32_t edge = triangle * 3U;
            triangles.push_back(
                {{canonical_ids[merged.indices[edge]], canonical_ids[merged.indices[edge + 1U]],
                  canonical_ids[merged.indices[edge + 2U]]},
                 {merged.external_edges[edge], merged.external_edges[edge + 1U], merged.external_edges[edge + 2U]},
                 merged.material_indices[triangle]});
        }

        std::vector<Quadric> quadrics(vertices.size());
        for (const Triangle& triangle : triangles)
        {
            const Math::Vec3 p0 = vertices[triangle.vertices[0]].position;
            const Math::Vec3 p1 = vertices[triangle.vertices[1]].position;
            const Math::Vec3 p2 = vertices[triangle.vertices[2]].position;
            Math::Vec3 normal = Math::Cross(p1 - p0, p2 - p0);
            if (!Math::Normalize(normal))
            {
                continue;
            }
            const double distance = -Math::Dot(normal, p0);
            for (std::uint32_t corner = 0; corner < 3; ++corner)
            {
                quadrics[triangle.vertices[corner]].AddPlane(normal, distance);
            }
        }

        double maximum_error = 0.0;
        while (triangles.size() > target_triangle_count)
        {
            const std::map<EdgeKey, EdgeInfo> edges = BuildEdges(triangles);
            std::vector<bool> locked(vertices.size());
            for (const auto& [key, info] : edges)
            {
                bool boundary = info.count != 2U;
                if (!boundary)
                {
                    const EdgeUse left = info.uses[0];
                    const EdgeUse right = info.uses[1];
                    const Triangle& left_triangle = triangles[left.triangle];
                    const Triangle& right_triangle = triangles[right.triangle];
                    boundary = left_triangle.material != right_triangle.material ||
                               left_triangle.external_edges[left.local_edge] != 0U ||
                               right_triangle.external_edges[right.local_edge] != 0U ||
                               left_triangle.vertices[left.local_edge] == right_triangle.vertices[right.local_edge];
                }
                if (boundary)
                {
                    locked[static_cast<std::uint32_t>(key >> 32U)] = true;
                    locked[static_cast<std::uint32_t>(key)] = true;
                }
            }

            std::vector<Candidate> candidates;
            candidates.reserve(edges.size());
            for (const auto& [key, info] : edges)
            {
                const std::uint32_t first = static_cast<std::uint32_t>(key >> 32U);
                const std::uint32_t second = static_cast<std::uint32_t>(key);
                if (info.count != 2U || locked[first] || locked[second])
                {
                    continue;
                }

                Quadric combined = quadrics[first];
                combined.Add(quadrics[second]);
                const Math::Vec3 p0 = vertices[first].position;
                const Math::Vec3 p1 = vertices[second].position;
                const std::array<Math::Vec3, 3> placements{p0, p1, (p0 + p1) * 0.5F};
                Candidate candidate{first, second, p0, std::numeric_limits<double>::max(), Math::LengthSquared(p1 - p0),
                                    0U};
                for (std::uint32_t placement = 0; placement < 3; ++placement)
                {
                    const double error = std::max(0.0, combined.Evaluate(placements[placement]));
                    if (error < candidate.error)
                    {
                        candidate.position = placements[placement];
                        candidate.error = error;
                        candidate.placement = placement;
                    }
                }
                candidates.push_back(candidate);
            }

            std::sort(candidates.begin(), candidates.end(),
                      [](const Candidate& left, const Candidate& right)
                      {
                          if (left.error != right.error)
                          {
                              return left.error < right.error;
                          }
                          if (left.edge_length_squared != right.edge_length_squared)
                          {
                              return left.edge_length_squared < right.edge_length_squared;
                          }
                          if (left.first != right.first)
                          {
                              return left.first < right.first;
                          }
                          return left.second < right.second;
                      });

            const auto selected = std::ranges::find_if(candidates, [&](const Candidate& candidate)
                                                       { return CanCollapse(candidate, triangles, vertices, edges); });
            if (selected == candidates.end())
            {
                break;
            }

            const Candidate collapse = *selected;
            maximum_error = std::max(maximum_error, collapse.error);
            AssetFormats::StaticMeshVertex& kept = vertices[collapse.first];
            const AssetFormats::StaticMeshVertex& removed = vertices[collapse.second];
            if (collapse.placement == 1U)
            {
                kept = removed;
            }
            else if (collapse.placement == 2U)
            {
                kept.position = collapse.position;
                kept.normal = (kept.normal + removed.normal) * 0.5F;
                static_cast<void>(Math::Normalize(kept.normal));
                for (std::uint32_t axis = 0; axis < 2U; ++axis)
                {
                    kept.texcoord[axis] = (kept.texcoord[axis] + removed.texcoord[axis]) * 0.5F;
                }
            }
            quadrics[collapse.first].Add(quadrics[collapse.second]);

            std::erase_if(triangles,
                          [&](Triangle& triangle)
                          {
                              const bool has_first =
                                  std::ranges::find(triangle.vertices, collapse.first) != triangle.vertices.end();
                              const bool has_second =
                                  std::ranges::find(triangle.vertices, collapse.second) != triangle.vertices.end();
                              if (has_first && has_second)
                              {
                                  return true;
                              }
                              for (std::uint32_t& vertex : triangle.vertices)
                              {
                                  if (vertex == collapse.second)
                                  {
                                      vertex = collapse.first;
                                  }
                              }
                              return false;
                          });
        }

        merged.indices.clear();
        merged.material_indices.clear();
        merged.external_edges.clear();
        std::vector<std::uint32_t> compact_ids(vertices.size(), std::numeric_limits<std::uint32_t>::max());
        merged.vertices.clear();
        for (const Triangle& triangle : triangles)
        {
            for (std::uint32_t corner = 0; corner < 3U; ++corner)
            {
                const std::uint32_t vertex = triangle.vertices[corner];
                if (compact_ids[vertex] == std::numeric_limits<std::uint32_t>::max())
                {
                    compact_ids[vertex] = static_cast<std::uint32_t>(merged.vertices.size());
                    merged.vertices.push_back(vertices[vertex]);
                }
                merged.indices.push_back(compact_ids[vertex]);
                merged.external_edges.push_back(triangle.external_edges[corner]);
            }
            merged.material_indices.push_back(triangle.material);
        }

        if (!merged.vertices.empty())
        {
            merged.bounds_min = merged.vertices[0].position;
            merged.bounds_max = merged.vertices[0].position;
            for (const auto& vertex : merged.vertices)
            {
                for (std::uint32_t axis = 0; axis < 3U; ++axis)
                {
                    merged.bounds_min[axis] = std::min(merged.bounds_min[axis], vertex.position[axis]);
                    merged.bounds_max[axis] = std::max(merged.bounds_max[axis], vertex.position[axis]);
                }
            }
        }
        return static_cast<float>(std::sqrt(maximum_error));
    }
} // namespace GE::Render::Nanite
