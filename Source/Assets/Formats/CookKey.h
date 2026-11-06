#pragma once

#include <Assets/Cook/CookedContainer.h>

#include <cstddef>
#include <cstdint>
#include <span>

namespace GE::AssetFormats
{
    struct CookKeyInput final
    {
        std::span<const std::byte> source_content;
        std::uint32_t importer_version = 0;
        std::uint32_t cooker_version = 0;
        std::span<const std::byte> settings;
        std::span<const Cook::ContentHash> dependency_hashes;
    };

    [[nodiscard]] Core::Result<Cook::ContentHash> BuildCookKey(const CookKeyInput& input);
} // namespace GE::AssetFormats
