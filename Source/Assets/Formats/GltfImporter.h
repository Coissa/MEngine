#pragma once

#include "StaticAssetFormats.h"

#include <Core/FileSystem/FileMount.h>

#include <optional>
#include <string>
#include <vector>

namespace GE::AssetFormats
{
    struct ImportedGltfMaterial final
    {
        MaterialAsset material;
        std::optional<std::string> base_color_texture_uri;
        std::optional<std::string> metallic_roughness_texture_uri;
        std::optional<std::string> normal_texture_uri;
        std::optional<std::string> emissive_texture_uri;
        std::optional<std::string> occlusion_texture_uri;
    };

    struct ImportedGltf final
    {
        StaticMeshAsset mesh;
        std::vector<ImportedGltfMaterial> materials;
    };

    [[nodiscard]] Core::Result<ImportedGltf> ImportGltf(const FileSystem::IReadOnlyFileMount& source_mount,
                                                        const FileSystem::LogicalPath& source_path);
} // namespace GE::AssetFormats
