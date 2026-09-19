#pragma once

#include "AssetRegistry.h"

#include <Engine/Core/Diagnostics/Result.h>

#include <filesystem>
#include <iosfwd>

namespace GE::Asset
{
    class AssetRegistrySerializer final
    {
    public:
        static constexpr unsigned int CurrentVersion = 1;

        [[nodiscard]] static Core::Result<AssetRegistry> Load(
            std::istream& input,
            AssetId project_namespace);
        [[nodiscard]] static Core::Result<AssetRegistry> LoadFromFile(
            const std::filesystem::path& path,
            AssetId project_namespace);
        [[nodiscard]] static Core::Result<void> Save(
            const AssetRegistry& registry,
            std::ostream& output);
        [[nodiscard]] static Core::Result<void> SaveToFile(
            const AssetRegistry& registry,
            const std::filesystem::path& path);
    };
}
