#include "Cluster.h"
#include "MortonCode.h"

#include <algorithm>
#include <bit>
#include <iterator>
#include <numeric>
#include <queue>
#include <unordered_map>
#include <utility>

namespace GE::Render::Nanite
{
    namespace
    {
        struct PositionKey final
        {
            std::uint32_t x;
            std::uint32_t y;
            std::uint32_t z;

            bool operator==(const PositionKey&) const noexcept = default;
        };

        struct EdgeKey final
        {
            PositionKey start;
            PositionKey end;

            bool operator==(const EdgeKey&) const noexcept = default;
        };

        struct EdgeKeyHash final
        {
            [[nodiscard]] std::size_t operator()(const EdgeKey& key) const noexcept
            {
                std::uint64_t hash = 1469598103934665603ULL;
                for (const std::uint32_t component :
                     {key.start.x, key.start.y, key.start.z, key.end.x, key.end.y, key.end.z})
                {
                    hash = (hash ^ component) * 1099511628211ULL;
                }
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

        [[nodiscard]] EdgeKey MakeEdgeKey(const Cluster& cluster, std::uint32_t edge_index) noexcept
        {
            const std::uint32_t triangle_begin = (edge_index / 3U) * 3U;
            const std::uint32_t local_edge = edge_index % 3U;
            const std::uint32_t next_edge = triangle_begin + ((local_edge + 1U) % 3U);

            return {MakePositionKey(cluster.vertices[cluster.indices[edge_index]].position),
                    MakePositionKey(cluster.vertices[cluster.indices[next_edge]].position)};
        }

        void AddClusterGraphEdge(ClusterGraph& graph, std::uint32_t from, std::uint32_t to)
        {
            for (ClusterGraphEdge& edge : graph[from])
            {
                if (edge.cluster == to)
                {
                    ++edge.shared_edge_count;
                    return;
                }
            }
            graph[from].push_back({to, 1U});
        }

        struct GroupCandidate final
        {
            std::uint32_t score;
            std::uint32_t morton_position;
            std::uint32_t cluster;
        };

        struct GroupCandidateLess final
        {
            [[nodiscard]] bool operator()(const GroupCandidate& left, const GroupCandidate& right) const noexcept
            {
                if (left.score != right.score)
                {
                    return left.score < right.score;
                }
                if (left.morton_position != right.morton_position)
                {
                    return left.morton_position > right.morton_position;
                }
                return left.cluster > right.cluster;
            }
        };
    } // namespace

    std::vector<std::uint32_t> BuildTriangleMaterialIndices(const AssetFormats::StaticMeshAsset& mesh)
    {
        std::vector<std::uint32_t> material_indices(mesh.indices.size() / 3U);

        for (const AssetFormats::StaticMeshSubmesh& submesh : mesh.submeshes)
        {
            const std::uint32_t triangle_begin = submesh.first_index / 3U;
            const std::uint32_t triangle_end = (submesh.first_index + submesh.index_count) / 3U;

            for (std::uint32_t triangle = triangle_begin; triangle < triangle_end; ++triangle)
            {
                material_indices[triangle] = submesh.material_slot;
            }
        }

        return material_indices;
    }

    [[nodiscard]] std::vector<Cluster> BuildLeafClusters(const AssetFormats::StaticMeshAsset& mesh,
                                                         const TriangleTopology& topology,
                                                         const TrianglePartition& partition,
                                                         std::span<const std::uint32_t> triangle_material_indices,
                                                         std::span<const std::uint8_t> source_external_edges)
    {
        std::vector<Cluster> Clusters;
        std::vector<LeafClusterTriangles> leaf_clusters;
        std::vector<std::uint32_t> triangle_cluster(mesh.indices.size() / 3U);
        std::vector<std::uint32_t> vertex_marks(mesh.vertices.size());
        std::uint32_t current_mark = 1;
        for (auto& range : partition.ranges)
        {
            const std::uint32_t& begin = range.begin;
            const std::uint32_t& end = range.end;
            LeafClusterTriangles leaf_cluster;
            std::uint32_t current_vertex_count = 0;

            for (std::uint32_t position = begin; position < end; ++position)
            {
                const std::uint32_t triangle = partition.indexes[position];
                const std::uint32_t vertex0 = mesh.indices[triangle * 3U];
                const std::uint32_t vertex1 = mesh.indices[triangle * 3U + 1U];
                const std::uint32_t vertex2 = mesh.indices[triangle * 3U + 2U];
                std::uint32_t new_vertex_count = 0;
                if (vertex_marks[vertex0] != current_mark)
                {
                    ++new_vertex_count;
                }
                if (vertex_marks[vertex1] != current_mark)
                {
                    ++new_vertex_count;
                }
                if (vertex_marks[vertex2] != current_mark)
                {
                    ++new_vertex_count;
                }

                if (leaf_cluster.triangles.size() == ClusterMaxTriangles ||
                    current_vertex_count + new_vertex_count > ClusterMaxVertices)
                {
                    leaf_clusters.push_back(leaf_cluster);
                    leaf_cluster = {};
                    current_vertex_count = 0;
                    ++current_mark;
                }

                if (vertex_marks[vertex0] != current_mark)
                {
                    vertex_marks[vertex0] = current_mark;
                    ++current_vertex_count;
                }
                if (vertex_marks[vertex1] != current_mark)
                {
                    vertex_marks[vertex1] = current_mark;
                    ++current_vertex_count;
                }
                if (vertex_marks[vertex2] != current_mark)
                {
                    vertex_marks[vertex2] = current_mark;
                    ++current_vertex_count;
                }
                triangle_cluster[triangle] = static_cast<std::uint32_t>(leaf_clusters.size());
                leaf_cluster.triangles.push_back(triangle);
            }
            leaf_clusters.push_back(leaf_cluster);
            ++current_mark;
        }

        std::fill(vertex_marks.begin(), vertex_marks.end(), 0U);
        std::vector<std::uint8_t> local_vertex_indices(mesh.vertices.size());
        std::uint32_t cluster_mark = 1;
        for (std::uint32_t cluster_index = 0; cluster_index < leaf_clusters.size(); ++cluster_index)
        {
            Cluster cluster = {};
            for (const std::uint32_t triangle : leaf_clusters[cluster_index].triangles)
            {
                for (std::uint32_t corner = 0; corner < 3U; ++corner)
                {
                    const std::uint32_t source_vertex = mesh.indices[triangle * 3U + corner];

                    if (vertex_marks[source_vertex] != cluster_mark)
                    {
                        vertex_marks[source_vertex] = cluster_mark;

                        const auto local_vertex = static_cast<std::uint8_t>(cluster.vertices.size());

                        local_vertex_indices[source_vertex] = local_vertex;
                        const AssetFormats::StaticMeshVertex& vertex = mesh.vertices[source_vertex];

                        if (cluster.vertices.empty())
                        {
                            cluster.bounds_min = vertex.position;
                            cluster.bounds_max = vertex.position;
                        }
                        else
                        {
                            for (std::uint32_t axis = 0; axis < 3U; ++axis)
                            {
                                cluster.bounds_min[axis] = std::min(cluster.bounds_min[axis], vertex.position[axis]);
                                cluster.bounds_max[axis] = std::max(cluster.bounds_max[axis], vertex.position[axis]);
                            }
                        }

                        cluster.vertices.push_back(vertex);
                    }

                    cluster.indices.push_back(local_vertex_indices[source_vertex]);

                    const std::uint32_t edge_index = triangle * 3U + corner;
                    std::uint32_t external_count =
                        source_external_edges.empty() ? 0U : source_external_edges[edge_index];
                    for (const std::uint32_t opposite_edge : topology.adjacent_edges[edge_index])
                    {
                        const std::uint32_t opposite_triangle = opposite_edge / 3U;
                        if (triangle_cluster[opposite_triangle] != cluster_index)
                        {
                            ++external_count;
                        }
                    }
                    cluster.external_edges.push_back(static_cast<std::uint8_t>(std::min(external_count, 255U)));
                }

                cluster.material_indices.push_back(triangle_material_indices[triangle]);
            }
            cluster.lod_center = (cluster.bounds_min + cluster.bounds_max) * 0.5F;
            cluster.lod_r = Math::Length(cluster.bounds_max - cluster.bounds_min) * 0.5F;
            Clusters.push_back(cluster);
            ++cluster_mark;
        }
        return Clusters;
    }

    [[nodiscard]] ClusterGraph BuildClusterGraph(std::span<const Cluster> clusters)
    {
        ClusterGraph graph(clusters.size());
        std::unordered_map<EdgeKey, std::vector<ClusterHalfEdge>, EdgeKeyHash> edge_buckets;

        for (std::uint32_t cluster_index = 0; cluster_index < clusters.size(); ++cluster_index)
        {
            const Cluster& cluster = clusters[cluster_index];
            for (std::uint32_t edge_index = 0; edge_index < cluster.external_edges.size(); ++edge_index)
            {
                if (cluster.external_edges[edge_index] != 0U)
                {
                    edge_buckets[MakeEdgeKey(cluster, edge_index)].push_back({cluster_index, edge_index});
                }
            }
        } // 建立边映射

        for (std::uint32_t cluster_index = 0; cluster_index < clusters.size(); ++cluster_index)
        {
            const Cluster& cluster = clusters[cluster_index];
            for (std::uint32_t edge_index = 0; edge_index < cluster.external_edges.size(); ++edge_index)
            {
                if (cluster.external_edges[edge_index] == 0U)
                {
                    continue;
                }

                const EdgeKey edge = MakeEdgeKey(cluster, edge_index);
                const auto bucket = edge_buckets.find({edge.end, edge.start});
                if (bucket == edge_buckets.end())
                {
                    continue;
                }

                for (const ClusterHalfEdge& opposite : bucket->second)
                {
                    if (opposite.cluster != cluster_index)
                    {
                        AddClusterGraphEdge(graph, cluster_index, opposite.cluster);
                    }
                }
            }
        } // 查找包围边的反向以查找相连cluster

        for (auto& edges : graph)
        {
            std::sort(edges.begin(), edges.end(), [](const ClusterGraphEdge& left, const ClusterGraphEdge& right)
                      { return left.cluster < right.cluster; });
        } // 按ID排序
        return graph;
    }

    [[nodiscard]] std::vector<ClusterGroup> GroupClusters(std::span<const Cluster> clusters, const ClusterGraph& graph)
    {
        const std::uint32_t cluster_count = static_cast<std::uint32_t>(clusters.size());

        std::vector<ClusterGroup> groups;
        if (cluster_count == 0)
        {
            return groups;
        }
        if (cluster_count <= ClusterGroupMaxSize)
        {
            groups.emplace_back();
            groups.back().children.resize(cluster_count);
            std::iota(groups.back().children.begin(), groups.back().children.end(), 0U);
            return groups;
        }

        std::vector<Math::Vec3> centers(cluster_count);
        for (std::uint32_t cluster = 0; cluster < cluster_count; ++cluster)
        {
            centers[cluster] = (clusters[cluster].bounds_min + clusters[cluster].bounds_max) * 0.5F;
        }

        Math::Vec3 bounds_min = centers[0];
        Math::Vec3 bounds_max = bounds_min;
        for (const Math::Vec3 center : centers)
        {
            for (std::uint32_t axis = 0; axis < 3U; ++axis)
            {
                bounds_min[axis] = std::min(bounds_min[axis], center[axis]);
                bounds_max[axis] = std::max(bounds_max[axis], center[axis]);
            }
        }

        const Math::Vec3 bounds_extent = bounds_max - bounds_min;
        const float maximum_extent = std::max(bounds_extent[0], std::max(bounds_extent[1], bounds_extent[2]));
        const float inverse_extent = maximum_extent > 0.0F ? 1.0F / maximum_extent : 0.0F;

        std::vector<std::uint32_t> morton_codes(cluster_count);
        std::vector<std::uint32_t> morton_order(cluster_count);
        std::iota(morton_order.begin(), morton_order.end(), 0U);
        for (std::uint32_t cluster = 0; cluster < cluster_count; ++cluster)
        {
            morton_codes[cluster] = CalculateMortonCode((centers[cluster] - bounds_min) * inverse_extent);
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

        std::vector<std::uint32_t> morton_position(cluster_count);
        for (std::uint32_t position = 0; position < cluster_count; ++position)
        {
            morton_position[morton_order[position]] = position;
        }

        const std::uint32_t group_count = (cluster_count + ClusterGroupMaxSize - 1U) / ClusterGroupMaxSize;
        const std::uint32_t base_group_size = cluster_count / group_count;
        const std::uint32_t larger_group_count = cluster_count % group_count;

        groups.reserve(group_count);
        std::vector<bool> assigned(cluster_count, false);
        std::vector<std::uint32_t> scores(cluster_count);
        std::uint32_t next_seed_position = 0;

        for (std::uint32_t group_index = 0; group_index < group_count; ++group_index)
        {
            const std::uint32_t target_size = base_group_size + (group_index < larger_group_count ? 1U : 0U);
            ClusterGroup group;
            group.children.reserve(target_size);
            std::fill(scores.begin(), scores.end(), 0U);

            std::priority_queue<GroupCandidate, std::vector<GroupCandidate>, GroupCandidateLess> candidates;

            while (assigned[morton_order[next_seed_position]])
            {
                ++next_seed_position;
            }

            const auto assign_cluster = [&](std::uint32_t cluster)
            {
                assigned[cluster] = true;
                group.children.push_back(cluster);

                for (const ClusterGraphEdge& edge : graph[cluster])
                {
                    if (!assigned[edge.cluster])
                    {
                        scores[edge.cluster] += edge.shared_edge_count * 16U + 4U;
                        candidates.push({scores[edge.cluster], morton_position[edge.cluster], edge.cluster});
                    }
                }
            };

            assign_cluster(morton_order[next_seed_position]);
            while (group.children.size() < target_size)
            {
                while (!candidates.empty() && (assigned[candidates.top().cluster] ||
                                               candidates.top().score != scores[candidates.top().cluster]))
                {
                    candidates.pop();
                }

                std::uint32_t next_cluster;
                if (!candidates.empty())
                {
                    next_cluster = candidates.top().cluster;
                    candidates.pop();
                }
                else
                {
                    while (assigned[morton_order[next_seed_position]])
                    {
                        ++next_seed_position;
                    }
                    next_cluster = morton_order[next_seed_position];
                }

                assign_cluster(next_cluster);
            }

            groups.push_back(std::move(group));
        }

        return groups;
    }

    [[nodiscard]] MergedCluster MergeClusterGroup(std::span<const Cluster> clusters, const ClusterGroup& group)
    {
        MergedCluster merged;
        merged.bounds_min = clusters[group.children.front()].bounds_min;
        merged.bounds_max = clusters[group.children.front()].bounds_max;

        std::vector<std::uint32_t> triangle_children;
        for (const std::uint32_t child_index : group.children)
        {
            const Cluster& child = clusters[child_index];
            const std::uint32_t vertex_offset = static_cast<std::uint32_t>(merged.vertices.size());

            merged.vertices.insert(merged.vertices.end(), child.vertices.begin(), child.vertices.end());
            for (const std::uint8_t index : child.indices)
            {
                merged.indices.push_back(vertex_offset + index);
            }
            merged.material_indices.insert(merged.material_indices.end(), child.material_indices.begin(),
                                           child.material_indices.end());
            merged.external_edges.insert(merged.external_edges.end(), child.external_edges.begin(),
                                         child.external_edges.end());
            triangle_children.insert(triangle_children.end(), child.indices.size() / 3U, child_index);

            for (std::uint32_t axis = 0; axis < 3U; ++axis)
            {
                merged.bounds_min[axis] = std::min(merged.bounds_min[axis], child.bounds_min[axis]);
                merged.bounds_max[axis] = std::max(merged.bounds_max[axis], child.bounds_max[axis]);
            }
        }

        if (group.children.size() == 1U)
        {
            return merged;
        }

        const TriangleTopologyInput input{.vertices = merged.vertices, .indices = merged.indices};
        const TriangleTopology topology = BuildTriangleTopology(input);

        for (std::uint32_t edge_index = 0; edge_index < merged.external_edges.size(); ++edge_index)
        {
            const std::uint32_t external_count = merged.external_edges[edge_index];
            if (external_count == 0)
            {
                continue;
            }

            std::uint32_t internal_count = 0;
            const std::uint32_t child_index = triangle_children[edge_index / 3U];
            for (const std::uint32_t opposite_edge : topology.adjacent_edges[edge_index])
            {
                internal_count += triangle_children[opposite_edge / 3U] != child_index;
            }
            merged.external_edges[edge_index] =
                static_cast<std::uint8_t>(external_count > internal_count ? external_count - internal_count : 0U);
        }

        return merged;
    }

    [[nodiscard]] std::vector<Cluster> BuildParentClusters(std::span<const Cluster> clusters,
                                                           std::span<ClusterGroup> groups,
                                                           std::span<const MergedCluster> merged_groups)
    {
        std::vector<Cluster> parents;
        for (std::uint32_t group_index = 0; group_index < groups.size(); ++group_index)
        {
            ClusterGroup& group = groups[group_index];
            if (group.children.size() == 1U)
            {
                continue;
            }

            MergedCluster merged = merged_groups[group_index];
            const std::uint32_t triangle_count = static_cast<std::uint32_t>(merged.indices.size() / 3U);
            const std::uint32_t parent_budget =
                (triangle_count + ClusterMaxTriangles * 2U - 1U) / (ClusterMaxTriangles * 2U);
            const std::uint32_t target_triangles = parent_budget * (ClusterMaxTriangles - 2U);

            const Cluster& first_child = clusters[group.children[0]];
            const Math::Vec3 first_radius{first_child.lod_r, first_child.lod_r, first_child.lod_r};
            Math::Vec3 lod_min = first_child.lod_center - first_radius;
            Math::Vec3 lod_max = first_child.lod_center + first_radius;
            float child_lod_error = 0.0F;
            for (const std::uint32_t child : group.children)
            {
                const Cluster& child_cluster = clusters[child];
                for (std::uint32_t axis = 0; axis < 3U; ++axis)
                {
                    lod_min[axis] = std::min(lod_min[axis], child_cluster.lod_center[axis] - child_cluster.lod_r);
                    lod_max[axis] = std::max(lod_max[axis], child_cluster.lod_center[axis] + child_cluster.lod_r);
                }
                child_lod_error = std::max(child_lod_error, child_cluster.lod_error);
            }
            group.lod_center = (lod_min + lod_max) * 0.5F;
            group.lod_r = 0.0F;
            for (const std::uint32_t child : group.children)
            {
                const Cluster& child_cluster = clusters[child];
                group.lod_r = std::max(group.lod_r,
                                       Math::Length(child_cluster.lod_center - group.lod_center) + child_cluster.lod_r);
            }
            group.parent_lod_error = std::max(child_lod_error, SimplifyMergedCluster(merged, target_triangles));
            group.parent_begin = static_cast<std::uint32_t>(parents.size());

            AssetFormats::StaticMeshAsset mesh;
            mesh.vertices = std::move(merged.vertices);
            mesh.indices = std::move(merged.indices);
            const TriangleTopology topology = BuildTriangleTopology(mesh);
            const TrianglePartition partition = PartitionTriangles(topology, merged.material_indices);
            std::vector<Cluster> split =
                BuildLeafClusters(mesh, topology, partition, merged.material_indices, merged.external_edges);
            for (Cluster& parent : split)
            {
                parent.lod_center = group.lod_center;
                parent.lod_r = group.lod_r;
                parent.lod_error = group.parent_lod_error;
            }

            group.parent_count = static_cast<std::uint32_t>(split.size());
            parents.insert(parents.end(), std::make_move_iterator(split.begin()), std::make_move_iterator(split.end()));
        }
        return parents;
    }

    [[nodiscard]] std::vector<NodeClusterLevel> BuildLeafClusterLevels(const AssetFormats::StaticMeshAsset& mesh,
                                                                       bool build_parents)
    {
        std::vector<NodeClusterLevel> levels;
        std::vector<std::uint32_t> vertex_marks(mesh.vertices.size());
        std::vector<std::uint32_t> local_vertex_indices(mesh.vertices.size());

        for (std::uint32_t node_index = 0; node_index < mesh.nodes.size(); ++node_index)
        {
            const AssetFormats::StaticMeshNode& node = mesh.nodes[node_index];
            if (node.submesh_count == 0)
            {
                continue;
            }

            AssetFormats::StaticMeshAsset local_mesh;
            local_mesh.submeshes.reserve(node.submesh_count);
            const std::uint32_t node_mark = node_index + 1U;

            for (std::uint32_t submesh_index = node.first_submesh;
                 submesh_index < node.first_submesh + node.submesh_count; ++submesh_index)
            {
                AssetFormats::StaticMeshSubmesh submesh = mesh.submeshes[submesh_index];
                const std::uint32_t source_index_begin = submesh.first_index;
                const std::uint32_t source_index_end = source_index_begin + submesh.index_count;
                submesh.first_index = static_cast<std::uint32_t>(local_mesh.indices.size());

                for (std::uint32_t index = source_index_begin; index < source_index_end; ++index)
                {
                    const std::uint32_t source_vertex = mesh.indices[index];
                    if (vertex_marks[source_vertex] != node_mark)
                    {
                        vertex_marks[source_vertex] = node_mark;
                        local_vertex_indices[source_vertex] = static_cast<std::uint32_t>(local_mesh.vertices.size());
                        local_mesh.vertices.push_back(mesh.vertices[source_vertex]);
                    }
                    local_mesh.indices.push_back(local_vertex_indices[source_vertex]);
                }
                local_mesh.submeshes.push_back(submesh);
            }

            const std::vector<std::uint32_t> material_indices = BuildTriangleMaterialIndices(local_mesh);
            const TriangleTopology topology = BuildTriangleTopology(local_mesh);
            const TrianglePartition partition = PartitionTriangles(topology, material_indices);

            NodeClusterLevel level;
            level.node_index = node_index;
            level.clusters = BuildLeafClusters(local_mesh, topology, partition, material_indices);

            if (build_parents)
            {
                ClusterGraph graph;
                if (level.clusters.size() > ClusterGroupMaxSize)
                {
                    graph = BuildClusterGraph(level.clusters);
                }
                level.groups = GroupClusters(level.clusters, graph);
                level.merged_groups.reserve(level.groups.size());
                for (const ClusterGroup& group : level.groups)
                {
                    level.merged_groups.push_back(MergeClusterGroup(level.clusters, group));
                }
                level.parent_clusters = BuildParentClusters(level.clusters, level.groups, level.merged_groups);
            }
            levels.push_back(std::move(level));
        }

        return levels;
    }

    std::vector<ClusterDAG> ReduceMesh(const AssetFormats::StaticMeshAsset& mesh)
    {
        std::vector<ClusterDAG> reduce_results;
        auto nodes = BuildLeafClusterLevels(mesh, false);
        for (auto& node : nodes)
        {
            ClusterDAG dag;
            dag.node_index = node.node_index;
            auto& nowlevel = node;
            LevelRange level_range;
            std::vector<Cluster> cluster = nowlevel.clusters;
            std::vector<MergedCluster> merged_groups;

            while (cluster.size() > 1)
            {
                ClusterGraph graph = BuildClusterGraph(cluster);
                std::vector<ClusterGroup> groups = GroupClusters(cluster, graph);
                merged_groups.clear();
                for (const auto& group : groups)
                {
                    merged_groups.push_back(MergeClusterGroup(cluster, group));
                }
                auto parent_clusters = BuildParentClusters(cluster, groups, merged_groups);
                if (parent_clusters.empty() || parent_clusters.size() >= cluster.size())
                {
                    break;
                }

                level_range.group_begin = static_cast<std::uint32_t>(dag.groups.size());
                level_range.group_count = static_cast<std::uint32_t>(groups.size());
                level_range.cluster_begin = static_cast<std::uint32_t>(dag.clusters.size());
                level_range.cluster_count = static_cast<std::uint32_t>(cluster.size());
                dag.levels.push_back(level_range);
                dag.groups.insert(dag.groups.end(), groups.begin(), groups.end());
                dag.clusters.insert(dag.clusters.end(), cluster.begin(), cluster.end());
                cluster = std::move(parent_clusters);
            }
            level_range.group_begin = static_cast<std::uint32_t>(dag.groups.size());
            level_range.group_count = 0;
            level_range.cluster_begin = static_cast<std::uint32_t>(dag.clusters.size());
            level_range.cluster_count = static_cast<std::uint32_t>(cluster.size());
            dag.levels.push_back(level_range);
            dag.clusters.insert(dag.clusters.end(), cluster.begin(), cluster.end());
            BuildClusterTraversalData(dag);
            reduce_results.push_back(std::move(dag));
        }
        return reduce_results;
    }

} // namespace GE::Render::Nanite
