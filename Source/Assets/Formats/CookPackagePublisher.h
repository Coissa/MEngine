#pragma once

#include <Assets/Registry/AssetRegistry.h>
#include <Core/FileSystem/FileMount.h>
#include <Core/FileSystem/LogicalPath.h>

#include <filesystem>
#include <span>

namespace GE::AssetFormats
{
    struct CookPackageFile final
    {
        FileSystem::LogicalPath path;
        FileSystem::ByteBuffer bytes;
    };

    [[nodiscard]] Core::Result<void> PublishCookPackageAtomically(const std::filesystem::path& target_directory,
                                                                  const Asset::AssetRegistry& registry,
                                                                  std::span<const CookPackageFile> files);
} // namespace GE::AssetFormats
