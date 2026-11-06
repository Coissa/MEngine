#pragma once

#include <Assets/Formats/StaticAssetFormats.h>
#include <Renderer/SceneData.h>

#include <cstdint>
#include <vector>

namespace GE::Editor
{
    struct ClusterDebugGeometry final
    {
        Render::RHINaniteMeshDescription mesh;
        std::vector<RenderCore::RenderItem> render_items;
        Math::Vec3 bounds_min{};
        Math::Vec3 bounds_max{};
        std::uint32_t cluster_count = 0;
    };

    [[nodiscard]] ClusterDebugGeometry BuildClusterDebugGeometry(const AssetFormats::StaticMeshAsset& mesh,
                                                                 Render::Nanite::NaniteMeshData data,
                                                                 std::uint32_t first_mesh_handle);
} // namespace GE::Editor
