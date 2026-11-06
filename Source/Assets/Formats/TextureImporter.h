#pragma once

#include "StaticAssetFormats.h"

#include <Core/FileSystem/FileMount.h>

namespace GE::AssetFormats
{
    enum class ImportedTextureColorSpace : std::uint8_t
    {
        Automatic,
        Linear,
        Srgb
    };

    struct TextureImportSettings final
    {
        ImportedTextureColorSpace color_space = ImportedTextureColorSpace::Automatic;
        bool generate_mips = true;
        bool normal_map = false;
    };

    // Rebuild from the base level; RGB is filtered in linear light, alpha remains linear.
    void GenerateTextureMips(TextureAsset& asset, bool normal_map = false);

    [[nodiscard]] Core::Result<TextureAsset> ImportTexture(const FileSystem::IReadOnlyFileMount& source_mount,
                                                           const FileSystem::LogicalPath& source_path,
                                                           TextureImportSettings settings = {});
} // namespace GE::AssetFormats
