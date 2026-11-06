#pragma once

#include <Assets/Formats/StaticAssetFormats.h>
#include <Core/Math/vector.h>

#include <cstdint>
#include <span>
#include <vector>

namespace GE::Render::Nanite
{
    struct TriangleTopologyInput final
    {
        std::span<const AssetFormats::StaticMeshVertex> vertices;
        std::span<const std::uint32_t> indices;

        // Optional. When present, contains one entry per triangle.
        std::span<const std::uint32_t> triangle_groups;
    };

    // Each entry stores opposite half-edge indices. Dividing a half-edge index
    // by three gives its owner triangle. Multiple entries support non-manifold
    // edges without silently overwriting neighbours in the hash table.
    struct TriangleTopology final
    {
        std::vector<std::vector<std::uint32_t>> adjacent_edges;
        std::vector<std::uint32_t> island_ids;
        std::uint32_t island_count = 0;
        std::vector<Math::Vec3> triangle_centers;
    };

    [[nodiscard]] TriangleTopology BuildTriangleTopology(const TriangleTopologyInput& input);

    [[nodiscard]] TriangleTopology BuildTriangleTopology(const AssetFormats::StaticMeshAsset& mesh);

} // namespace GE::Render::Nanite
