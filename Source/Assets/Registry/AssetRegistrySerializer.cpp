#include "AssetRegistrySerializer.h"

#include "AssetFile.h"

#include <fstream>
#include <iomanip>
#include <limits>
#include <string>

namespace GE::Asset
{
    namespace
    {
        constexpr std::size_t MaximumAssetCount = 1'000'000;
        constexpr std::size_t MaximumDependencyCount = 1'000'000;
        constexpr std::size_t MaximumTextLength = 16 * 1024;

        [[nodiscard]] Core::EngineError RegistryError(Core::ErrorCode code, std::string operation, std::string message,
                                                      std::optional<std::string> resource = std::nullopt)
        {
            return {.domain =
                        code == Core::ErrorCode::IoFailure ? Core::ErrorDomain::Filesystem : Core::ErrorDomain::Asset,
                    .code = code,
                    .operation = std::move(operation),
                    .message = std::move(message),
                    .resource = std::move(resource)};
        }

        [[nodiscard]] bool ReadToken(std::istream& input, std::string_view expected)
        {
            std::string actual;
            return (input >> actual) && actual == expected;
        }

        [[nodiscard]] bool HasValidLength(std::string_view text) noexcept
        {
            return text.size() <= MaximumTextLength;
        }
    } // namespace

    Core::Result<AssetRegistry> AssetRegistrySerializer::Load(std::istream& input, AssetId project_namespace)
    {
        if (!project_namespace.IsValid())
        {
            return Core::Result<AssetRegistry>::Failure(RegistryError(
                Core::ErrorCode::InvalidArgument, "LoadAssetRegistry", "Project namespace UUID is invalid"));
        }

        std::string magic;
        unsigned int version = 0;
        std::size_t asset_count = 0;
        if (!(input >> magic >> version) || magic != "MENGINE_ASSET_REGISTRY")
        {
            return Core::Result<AssetRegistry>::Failure(
                RegistryError(Core::ErrorCode::ParseFailure, "LoadAssetRegistry", "Invalid asset registry header"));
        }
        if (version != CurrentVersion)
        {
            return Core::Result<AssetRegistry>::Failure(RegistryError(
                Core::ErrorCode::UnsupportedVersion, "LoadAssetRegistry", "Unsupported asset registry version"));
        }
        if (!ReadToken(input, "asset_count") || !(input >> asset_count) || asset_count > MaximumAssetCount)
        {
            return Core::Result<AssetRegistry>::Failure(
                RegistryError(Core::ErrorCode::ParseFailure, "LoadAssetRegistry", "Invalid asset count"));
        }

        AssetRegistry registry(project_namespace);
        for (std::size_t asset_index = 0; asset_index < asset_count; ++asset_index)
        {
            std::string type_name;
            std::string source_path;
            std::string subasset_key;
            std::string cooked_path;
            std::uint32_t importer_version = 0;
            std::uint32_t cooked_version = 0;
            std::size_t dependency_count = 0;
            if (!ReadToken(input, "asset") ||
                !(input >> std::quoted(type_name) >> std::quoted(source_path) >> std::quoted(subasset_key) >>
                  std::quoted(cooked_path) >> importer_version >> cooked_version >> dependency_count) ||
                dependency_count > MaximumDependencyCount || !HasValidLength(type_name) ||
                !HasValidLength(source_path) || !HasValidLength(subasset_key) || !HasValidLength(cooked_path))
            {
                return Core::Result<AssetRegistry>::Failure(
                    RegistryError(Core::ErrorCode::ParseFailure, "LoadAssetRegistry", "Invalid asset record"));
            }

            auto key = MakeAssetKey(source_path, subasset_key);
            if (!key || key.Value().SourcePath() != source_path)
            {
                return Core::Result<AssetRegistry>::Failure(
                    key ? RegistryError(Core::ErrorCode::ParseFailure, "LoadAssetRegistry",
                                        "Source path is not normalized", source_path)
                        : key.Error());
            }

            std::optional<std::string> normalized_cooked;
            if (!cooked_path.empty())
            {
                auto normalized = NormalizeAssetRelativePath(cooked_path);
                if (!normalized || normalized.Value() != cooked_path)
                {
                    return Core::Result<AssetRegistry>::Failure(
                        normalized ? RegistryError(Core::ErrorCode::ParseFailure, "LoadAssetRegistry",
                                                   "Cooked path is not normalized", cooked_path)
                                   : normalized.Error());
                }
                normalized_cooked = std::move(normalized).Value();
            }

            std::vector<AssetKey> dependencies;
            dependencies.reserve(dependency_count);
            for (std::size_t dependency_index = 0; dependency_index < dependency_count; ++dependency_index)
            {
                std::string dependency_path;
                std::string dependency_subasset;
                if (!ReadToken(input, "dependency") ||
                    !(input >> std::quoted(dependency_path) >> std::quoted(dependency_subasset)) ||
                    !HasValidLength(dependency_path) || !HasValidLength(dependency_subasset))
                {
                    return Core::Result<AssetRegistry>::Failure(
                        RegistryError(Core::ErrorCode::ParseFailure, "LoadAssetRegistry", "Invalid dependency record"));
                }
                auto dependency = MakeAssetKey(dependency_path, dependency_subasset);
                if (!dependency || dependency.Value().SourcePath() != dependency_path)
                {
                    return Core::Result<AssetRegistry>::Failure(
                        dependency ? RegistryError(Core::ErrorCode::ParseFailure, "LoadAssetRegistry",
                                                   "Dependency path is not normalized", dependency_path)
                                   : dependency.Error());
                }
                dependencies.push_back(std::move(dependency).Value());
            }
            if (!ReadToken(input, "end_asset"))
            {
                return Core::Result<AssetRegistry>::Failure(
                    RegistryError(Core::ErrorCode::ParseFailure, "LoadAssetRegistry", "Expected end_asset token"));
            }

            auto added = registry.Add({.key = std::move(key).Value(),
                                       .stable_type_name = std::move(type_name),
                                       .cooked_relative_path = std::move(normalized_cooked),
                                       .importer_version = importer_version,
                                       .cooked_version = cooked_version,
                                       .dependencies = std::move(dependencies)});
            if (!added)
            {
                return Core::Result<AssetRegistry>::Failure(added.Error());
            }
        }
        if (!ReadToken(input, "end_registry"))
        {
            return Core::Result<AssetRegistry>::Failure(
                RegistryError(Core::ErrorCode::ParseFailure, "LoadAssetRegistry", "Expected end_registry token"));
        }
        input >> std::ws;
        if (!input.eof())
        {
            return Core::Result<AssetRegistry>::Failure(RegistryError(
                Core::ErrorCode::ParseFailure, "LoadAssetRegistry", "Unexpected trailing asset registry data"));
        }
        auto frozen = registry.Freeze();
        if (!frozen)
        {
            return Core::Result<AssetRegistry>::Failure(frozen.Error());
        }
        return Core::Result<AssetRegistry>::Success(std::move(registry));
    }

    Core::Result<AssetRegistry> AssetRegistrySerializer::LoadFromFile(const std::filesystem::path& path,
                                                                      AssetId project_namespace)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            return Core::Result<AssetRegistry>::Failure(
                RegistryError(Core::ErrorCode::NotFound, "LoadAssetRegistryFile", "Could not open asset registry",
                              path.generic_string()));
        }
        auto result = Load(input, project_namespace);
        if (!result && !result.Error().resource.has_value())
        {
            result.Error().resource = path.generic_string();
        }
        return result;
    }

    Core::Result<void> AssetRegistrySerializer::Save(const AssetRegistry& registry, std::ostream& output)
    {
        if (!registry.IsFrozen())
        {
            return Core::Result<void>::Failure(RegistryError(Core::ErrorCode::InvalidState, "SaveAssetRegistry",
                                                             "Only a frozen asset registry can be saved"));
        }

        const auto metadata = registry.GetAll();
        output << "MENGINE_ASSET_REGISTRY " << CurrentVersion << '\n' << "asset_count " << metadata.size() << '\n';
        for (const AssetMetadata& asset : metadata)
        {
            output << "asset " << std::quoted(asset.stable_type_name) << ' ' << std::quoted(asset.key.SourcePath())
                   << ' ' << std::quoted(asset.key.SubassetKey()) << ' '
                   << std::quoted(asset.cooked_relative_path.value_or("")) << ' ' << asset.importer_version << ' '
                   << asset.cooked_version << ' ' << asset.dependencies.size() << '\n';
            for (const AssetId dependency_id : asset.dependencies)
            {
                const AssetMetadata* dependency = registry.Find(dependency_id);
                if (dependency == nullptr)
                {
                    return Core::Result<void>::Failure(RegistryError(Core::ErrorCode::InvalidState, "SaveAssetRegistry",
                                                                     "Frozen registry has an unresolved dependency"));
                }
                output << "dependency " << std::quoted(dependency->key.SourcePath()) << ' '
                       << std::quoted(dependency->key.SubassetKey()) << '\n';
            }
            output << "end_asset\n";
        }
        output << "end_registry\n";
        if (!output)
        {
            return Core::Result<void>::Failure(
                RegistryError(Core::ErrorCode::IoFailure, "SaveAssetRegistry", "Could not write asset registry"));
        }
        return Core::Result<void>::Success();
    }

    Core::Result<void> AssetRegistrySerializer::SaveToFile(const AssetRegistry& registry,
                                                           const std::filesystem::path& path)
    {
        const std::filesystem::path temporary = path.string() + ".tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            return Core::Result<void>::Failure(RegistryError(Core::ErrorCode::IoFailure, "SaveAssetRegistryFile",
                                                             "Could not open temporary asset registry",
                                                             temporary.generic_string()));
        }
        auto saved = Save(registry, output);
        output.flush();
        const bool stream_ok = static_cast<bool>(output);
        output.close();
        if (!saved || !stream_ok)
        {
            Detail::RemoveTemporaryFile(temporary);
            if (!saved)
                return saved;
            return Core::Result<void>::Failure(RegistryError(Core::ErrorCode::IoFailure, "SaveAssetRegistryFile",
                                                             "Could not flush asset registry",
                                                             temporary.generic_string()));
        }
        auto replaced = Detail::ReplaceFileAtomically(temporary, path);
        if (!replaced)
            Detail::RemoveTemporaryFile(temporary);
        return replaced;
    }
} // namespace GE::Asset
