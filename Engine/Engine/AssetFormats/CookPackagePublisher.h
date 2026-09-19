#pragma once

#include <Engine/Asset/AssetRegistry.h>
#include <Engine/FileSystem/FileMount.h>
#include <Engine/FileSystem/LogicalPath.h>

#include <filesystem>
#include <span>

namespace GE::AssetFormats
{
    struct CookPackageFile final
    {
        FileSystem::LogicalPath path;
        FileSystem::ByteBuffer bytes;
    };

    [[nodiscard]] Core::Result<void> PublishCookPackageAtomically(
        const std::filesystem::path& target_directory,
        const Asset::AssetRegistry& registry,
        std::span<const CookPackageFile> files);
}
