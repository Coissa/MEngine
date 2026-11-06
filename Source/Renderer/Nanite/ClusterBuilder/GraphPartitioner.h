#pragma once

#include "ClusterDAG.h"

#include <cstdint>
#include <span>
#include <vector>

namespace GE::Render::Nanite
{
    struct PartitionRange final
    {
        std::uint32_t begin = 0;
        std::uint32_t end = 0;
    };

    struct TrianglePartition final
    {
        // Partition position -> source triangle ID.
        std::vector<std::uint32_t> indexes;

        // Source triangle ID -> partition position.
        std::vector<std::uint32_t> sorted_to;

        std::vector<PartitionRange> ranges;
    };

    [[nodiscard]] TrianglePartition PartitionTriangles(const TriangleTopology& topology,
                                                       std::span<const std::uint32_t> material_indices);
} // namespace GE::Render::Nanite
