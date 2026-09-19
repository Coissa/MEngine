#pragma once

#include "CookedContainer.h"

#include <filesystem>

namespace GE::Cook
{
    struct CookedSectionSource final
    {
        std::uint32_t type = 0;
        std::uint32_t flags = 0;
        std::span<const std::byte> bytes;
    };

    struct CookedContainerWriteRequest final
    {
        Asset::AssetTypeId asset_type;
        std::uint32_t payload_version = 0;
        std::span<const CookedSectionSource> sections;
    };

    [[nodiscard]] Core::Result<FileSystem::ByteBuffer> BuildCookedContainer(
        const CookedContainerWriteRequest& request);

    [[nodiscard]] Core::Result<void> PublishCookedContainer(
        const CookedContainerWriteRequest& request,
        const std::filesystem::path& target_path);
}
