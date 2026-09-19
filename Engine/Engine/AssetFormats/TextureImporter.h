#pragma once

#include "StaticAssetFormats.h"

#include <Engine/FileSystem/FileMount.h>

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
        ImportedTextureColorSpace color_space =
            ImportedTextureColorSpace::Automatic;
        bool generate_mips = true;
    };

    [[nodiscard]] Core::Result<TextureAsset> ImportTexture(
        const FileSystem::IReadOnlyFileMount& source_mount,
        const FileSystem::LogicalPath& source_path,
        TextureImportSettings settings = {});
}
