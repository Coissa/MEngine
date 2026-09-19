#include "CookPackagePublisher.h"

#include <Engine/Asset/AssetRegistrySerializer.h>
#include <Engine/Cook/CookedContainer.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <fstream>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace GE::AssetFormats
{
    namespace
    {
        [[nodiscard]] Core::EngineError PackageError(
            Core::ErrorCode code,
            std::string message,
            std::optional<std::string> resource = std::nullopt)
        {
            return {
                .domain = code == Core::ErrorCode::IoFailure
                    ? Core::ErrorDomain::Filesystem
                    : Core::ErrorDomain::Asset,
                .code = code,
                .operation = "PublishCookPackageAtomically",
                .message = std::move(message),
                .resource = std::move(resource)};
        }

        [[nodiscard]] std::string CanonicalPackagePath(
            std::string_view path)
        {
            std::string canonical(path);
            std::transform(canonical.begin(), canonical.end(),
                canonical.begin(), [](unsigned char value)
                {
                    return static_cast<char>(std::tolower(value));
                });
            return canonical;
        }

        class StagingDirectory final
        {
        public:
            explicit StagingDirectory(std::filesystem::path path)
                : m_path(std::move(path))
            {
            }
            ~StagingDirectory()
            {
                if (!m_path.empty())
                {
                    std::error_code ignored;
                    std::filesystem::remove_all(m_path, ignored);
                }
            }
            void Release() noexcept { m_path.clear(); }

        private:
            std::filesystem::path m_path;
        };

        [[nodiscard]] Core::Result<std::filesystem::path> CreateStaging(
            const std::filesystem::path& target)
        {
            static std::atomic_uint64_t sequence{0};
            const auto stamp = std::chrono::steady_clock::now()
                .time_since_epoch().count();
            const std::filesystem::path parent = target.parent_path().empty()
                ? std::filesystem::path(".") : target.parent_path();
            std::error_code error;
            std::filesystem::create_directories(parent, error);
            if (error)
            {
                return Core::Result<std::filesystem::path>::Failure(
                    PackageError(Core::ErrorCode::IoFailure,
                        "Could not create package parent directory",
                        parent.generic_string()));
            }
            for (unsigned int attempt = 0; attempt < 16; ++attempt)
            {
                const std::filesystem::path staging = parent /
                    (target.filename().string() + ".staging-"
                        + std::to_string(stamp) + "-"
                        + std::to_string(sequence.fetch_add(1)));
                error.clear();
                if (std::filesystem::create_directory(staging, error))
                    return Core::Result<std::filesystem::path>::Success(staging);
                if (error && error != std::errc::file_exists)
                {
                    return Core::Result<std::filesystem::path>::Failure(
                        PackageError(Core::ErrorCode::IoFailure,
                            "Could not create package staging directory",
                            staging.generic_string()));
                }
            }
            return Core::Result<std::filesystem::path>::Failure(PackageError(
                Core::ErrorCode::Conflict,
                "Could not allocate a unique package staging directory"));
        }

        [[nodiscard]] Core::Result<void> WriteFile(
            const std::filesystem::path& path,
            std::span<const std::byte> bytes)
        {
            if (bytes.size() > static_cast<std::size_t>(
                    std::numeric_limits<std::streamsize>::max()))
            {
                return Core::Result<void>::Failure(PackageError(
                    Core::ErrorCode::InvalidArgument,
                    "Cooked file is too large for the output stream",
                    path.generic_string()));
            }
            std::error_code error;
            std::filesystem::create_directories(path.parent_path(), error);
            if (error)
            {
                return Core::Result<void>::Failure(PackageError(
                    Core::ErrorCode::IoFailure,
                    "Could not create cooked file directory",
                    path.parent_path().generic_string()));
            }
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                return Core::Result<void>::Failure(PackageError(
                    Core::ErrorCode::IoFailure,
                    "Could not open cooked file", path.generic_string()));
            }
            output.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
            output.flush();
            if (!output)
            {
                return Core::Result<void>::Failure(PackageError(
                    Core::ErrorCode::IoFailure,
                    "Could not write cooked file", path.generic_string()));
            }
            return Core::Result<void>::Success();
        }
    }

    Core::Result<void> PublishCookPackageAtomically(
        const std::filesystem::path& target_directory,
        const Asset::AssetRegistry& registry,
        std::span<const CookPackageFile> files)
    {
        if (!registry.IsFrozen())
        {
            return Core::Result<void>::Failure(PackageError(
                Core::ErrorCode::InvalidState,
                "Only a frozen asset registry can be published"));
        }
        if (target_directory.empty() || target_directory.filename().empty()
            || target_directory.filename() == "."
            || target_directory.filename() == "..")
        {
            return Core::Result<void>::Failure(PackageError(
                Core::ErrorCode::InvalidArgument,
                "Package target must name a directory"));
        }

        std::error_code error;
        const bool target_exists = std::filesystem::exists(
            target_directory, error);
        if (error)
        {
            return Core::Result<void>::Failure(PackageError(
                Core::ErrorCode::IoFailure,
                "Could not inspect package target",
                target_directory.generic_string()));
        }
        if (target_exists)
        {
            return Core::Result<void>::Failure(PackageError(
                Core::ErrorCode::Conflict,
                "Immutable package target already exists",
                target_directory.generic_string()));
        }

        std::unordered_map<std::string, const CookPackageFile*> by_path;
        std::unordered_set<std::string> filesystem_paths;
        by_path.reserve(files.size());
        filesystem_paths.reserve(files.size());
        for (const CookPackageFile& file : files)
        {
            if (file.path.Empty() || file.bytes.empty())
            {
                return Core::Result<void>::Failure(PackageError(
                    Core::ErrorCode::InvalidArgument,
                    "Cooked package files require a path and bytes"));
            }
            const std::string filesystem_path = CanonicalPackagePath(
                file.path.String());
            if (filesystem_path == "assetregistry.mregistry")
            {
                return Core::Result<void>::Failure(PackageError(
                    Core::ErrorCode::Conflict,
                    "Cooked path conflicts with the package registry",
                    file.path.String()));
            }
            if (!filesystem_paths.insert(filesystem_path).second)
            {
                return Core::Result<void>::Failure(PackageError(
                    Core::ErrorCode::Conflict,
                    "Cooked package paths collide on a case-insensitive filesystem",
                    file.path.String()));
            }
            if (!by_path.emplace(file.path.String(), &file).second)
            {
                return Core::Result<void>::Failure(PackageError(
                    Core::ErrorCode::Conflict,
                    "Duplicate cooked package path", file.path.String()));
            }
            auto parsed = Cook::ParseCookedContainer(file.bytes);
            if (!parsed)
                return Core::Result<void>::Failure(parsed.Error());
        }

        const auto metadata = registry.GetAll();
        if (metadata.size() != files.size())
        {
            return Core::Result<void>::Failure(PackageError(
                Core::ErrorCode::InvalidArgument,
                "Registry and cooked file counts do not match"));
        }
        std::unordered_set<std::string> matched_paths;
        matched_paths.reserve(metadata.size());
        for (const Asset::AssetMetadata& asset : metadata)
        {
            if (!asset.cooked_relative_path.has_value())
            {
                return Core::Result<void>::Failure(PackageError(
                    Core::ErrorCode::InvalidArgument,
                    "Every packaged asset requires a cooked path",
                    asset.key.SourcePath()));
            }
            if (!matched_paths.insert(*asset.cooked_relative_path).second)
            {
                return Core::Result<void>::Failure(PackageError(
                    Core::ErrorCode::Conflict,
                    "Multiple registry assets refer to the same cooked path",
                    *asset.cooked_relative_path));
            }
            const auto file_it = by_path.find(*asset.cooked_relative_path);
            if (file_it == by_path.end())
            {
                return Core::Result<void>::Failure(PackageError(
                    Core::ErrorCode::NotFound,
                    "Registry cooked path is missing from the package",
                    *asset.cooked_relative_path));
            }
            auto parsed = Cook::ParseCookedContainer(file_it->second->bytes);
            if (!parsed)
                return Core::Result<void>::Failure(parsed.Error());
            if (parsed.Value().AssetType() != asset.type_id
                || parsed.Value().PayloadVersion() != asset.cooked_version)
            {
                return Core::Result<void>::Failure(PackageError(
                    Core::ErrorCode::InvalidArgument,
                    "Cooked container type or version does not match registry",
                    *asset.cooked_relative_path));
            }
        }

        auto staging_result = CreateStaging(target_directory);
        if (!staging_result)
            return Core::Result<void>::Failure(staging_result.Error());
        const std::filesystem::path staging = staging_result.Value();
        StagingDirectory cleanup(staging);
        for (const CookPackageFile& file : files)
        {
            auto written = WriteFile(staging / std::filesystem::path(
                file.path.String()), file.bytes);
            if (!written) return written;
        }
        auto registry_saved = Asset::AssetRegistrySerializer::SaveToFile(
            registry, staging / "AssetRegistry.mregistry");
        if (!registry_saved) return registry_saved;

        error.clear();
        std::filesystem::rename(staging, target_directory, error);
        if (error)
        {
            return Core::Result<void>::Failure(PackageError(
                Core::ErrorCode::IoFailure,
                "Could not atomically publish package directory",
                target_directory.generic_string()));
        }
        cleanup.Release();
        return Core::Result<void>::Success();
    }
}
