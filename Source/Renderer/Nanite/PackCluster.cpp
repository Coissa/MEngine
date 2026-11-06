#include "PackCluster.h"

namespace GE::Render::Nanite
{

    [[nodiscard]] NaniteMeshData PackClusterDAG(std::span<const ClusterDAG> dags)
    {
        NaniteMeshData mesh_data;

        for (const ClusterDAG& dag : dags)
        {
            const std::uint32_t group_base = static_cast<std::uint32_t>(mesh_data.groups.size());
            const std::uint32_t cluster_base = static_cast<std::uint32_t>(mesh_data.clusters.size());
            // Pack clusters
            for (const Cluster& cluster : dag.clusters)
            {
                ClusterDesc cluster_desc;
                cluster_desc.vertex_begin = static_cast<uint32_t>(mesh_data.vertices.size());
                cluster_desc.vertex_count = static_cast<uint32_t>(cluster.vertices.size());
                cluster_desc.index_begin = static_cast<uint32_t>(mesh_data.indices.size());
                cluster_desc.index_count = static_cast<uint32_t>(cluster.indices.size());
                cluster_desc.generating_group_index = cluster.generating_group_index == UINT32_MAX
                                                          ? UINT32_MAX
                                                          : group_base + cluster.generating_group_index;
                cluster_desc.group_index =
                    cluster.group_index == UINT32_MAX ? UINT32_MAX : group_base + cluster.group_index;
                cluster_desc.node_index = dag.node_index;
                cluster_desc.bounds_min = cluster.bounds_min;
                cluster_desc.bounds_max = cluster.bounds_max;
                cluster_desc.lod_center = cluster.lod_center;
                cluster_desc.lod_r = cluster.lod_r;
                cluster_desc.lod_error = cluster.lod_error;

                mesh_data.clusters.push_back(cluster_desc);
                mesh_data.vertices.insert(mesh_data.vertices.end(), cluster.vertices.begin(), cluster.vertices.end());
                mesh_data.indices.insert(mesh_data.indices.end(), cluster.indices.begin(), cluster.indices.end());
            }

            // Pack groups
            for (std::uint32_t level = 0; level + 1 < dag.levels.size(); ++level)
            {
                const LevelRange& current = dag.levels[level];
                const LevelRange& next = dag.levels[level + 1];
                const std::uint32_t group_begin = current.group_begin;
                const std::uint32_t group_end = current.group_begin + current.group_count;
                for (std::uint32_t group_index = group_begin; group_index < group_end; ++group_index)
                {
                    const ClusterGroup& group = dag.groups[group_index];
                    GroupDesc group_desc;
                    group_desc.children_begin = static_cast<uint32_t>(mesh_data.children.size());
                    group_desc.children_count = static_cast<uint32_t>(group.children.size());
                    group_desc.parent_begin = cluster_base + next.cluster_begin + group.parent_begin;
                    group_desc.parent_count = group.parent_count;
                    group_desc.lod_center = group.lod_center;
                    group_desc.lod_r = group.lod_r;
                    group_desc.parent_lod_error = group.parent_lod_error;
                    mesh_data.groups.push_back(group_desc);
                    for (const uint32_t child : group.children)
                    {
                        mesh_data.children.push_back(cluster_base + current.cluster_begin + child);
                    }
                }
            }
            // Pack node descriptor
            NodeDesc node_desc;
            const LevelRange& last_level = dag.levels.back();
            node_desc.node_index = dag.node_index;
            node_desc.root_begin = cluster_base + last_level.cluster_begin;
            node_desc.root_count = last_level.cluster_count;

            mesh_data.nodes.push_back(node_desc);
        }

        return mesh_data;
    }
} // namespace GE::Render::Nanite
