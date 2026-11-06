#include "ClusterDebugColorizer.h"

#include <Core/Math/matrix4.h>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace GE::Editor
{
    ClusterDebugGeometry BuildClusterDebugGeometry(const AssetFormats::StaticMeshAsset& source,
                                                   Render::Nanite::NaniteMeshData data, std::uint32_t first_mesh_handle)
    {
        constexpr RenderCore::MaterialHandle DebugMaterial{1U};

        ClusterDebugGeometry result;
        std::vector<Math::Mat4> node_world(source.nodes.size());
        for (std::uint32_t node = 0; node < source.nodes.size(); ++node)
        {
            const AssetFormats::StaticMeshNode& source_node = source.nodes[node];
            node_world[node] = source_node.parent < 0 ? source_node.local_transform
                                                      : source_node.local_transform *
                                                            node_world[static_cast<std::uint32_t>(source_node.parent)];
        }

        for (std::uint32_t node = 0; node < data.nodes.size(); ++node)
        {
            result.render_items.push_back({.mesh = {first_mesh_handle + node},
                                           .material = DebugMaterial,
                                           .object_id = std::uint64_t(node) + 1,
                                           .local_to_world = node_world[data.nodes[node].node_index]});
        }

        bool has_bounds = false;
        for (const Render::Nanite::ClusterDesc& cluster : data.clusters)
        {
            if (cluster.generating_group_index != UINT32_MAX)
                continue;
            for (std::uint32_t vertex = 0; vertex < cluster.vertex_count; ++vertex)
            {
                const Math::Vec3 world_position = Math::TransformPoint(
                    data.vertices[cluster.vertex_begin + vertex].position, node_world[cluster.node_index]);
                if (!has_bounds)
                {
                    result.bounds_min = world_position;
                    result.bounds_max = world_position;
                    has_bounds = true;
                }
                else
                {
                    for (std::uint32_t axis = 0; axis < 3U; ++axis)
                    {
                        result.bounds_min[axis] = std::min(result.bounds_min[axis], world_position[axis]);
                        result.bounds_max[axis] = std::max(result.bounds_max[axis], world_position[axis]);
                    }
                }
            }
            ++result.cluster_count;
        }
        result.mesh = {.first_mesh_handle = first_mesh_handle, .data = std::move(data)};
        return result;
    }
} // namespace GE::Editor
