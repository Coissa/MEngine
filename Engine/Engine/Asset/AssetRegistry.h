#pragma once

#include "AssetId.h"
#include "AssetKey.h"
#include "AssetTypeId.h"

#include <Engine/Core/Diagnostics/Result.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace GE::Asset
{
    struct AssetRecord final
    {
        AssetKey key;
        std::string stable_type_name;
        std::optional<std::string> cooked_relative_path;
        std::uint32_t importer_version = 0;
        std::uint32_t cooked_version = 0;
        std::vector<AssetKey> dependencies;
    };

    struct AssetMetadata final
    {
        AssetId id;
        AssetKey key;
        AssetTypeId type_id;
        std::string stable_type_name;
        std::optional<std::string> cooked_relative_path;
        std::uint32_t importer_version = 0;
        std::uint32_t cooked_version = 0;
        std::vector<AssetId> dependencies;
    };

    class AssetRegistry final
    {
    public:
        explicit AssetRegistry(AssetId project_namespace) noexcept;

        [[nodiscard]] Core::Result<AssetId> Add(AssetRecord record);
        [[nodiscard]] Core::Result<void> Freeze();

        [[nodiscard]] bool IsFrozen() const noexcept;
        [[nodiscard]] AssetId GetProjectNamespace() const noexcept;
        [[nodiscard]] const AssetMetadata* Find(AssetId id) const noexcept;
        [[nodiscard]] const AssetMetadata* Find(
            const AssetKey& key) const noexcept;
        [[nodiscard]] std::span<const AssetMetadata> GetAll() const noexcept;
        [[nodiscard]] std::span<const AssetId> GetDependencies(
            AssetId id) const noexcept;
        [[nodiscard]] std::span<const AssetId> GetReferencers(
            AssetId id) const noexcept;

    private:
        struct PendingEntry final
        {
            AssetId id;
            AssetTypeId type_id;
            AssetRecord record;
        };

        AssetId m_project_namespace;
        bool m_frozen = false;
        std::vector<PendingEntry> m_pending;
        std::vector<AssetMetadata> m_metadata;
        std::vector<std::vector<AssetId>> m_referencers;
        std::unordered_map<AssetId, std::size_t> m_id_indices;
        std::unordered_map<AssetKey, std::size_t> m_key_indices;
        std::unordered_map<AssetTypeId, std::string> m_type_names;
#ifdef _WIN32
        std::unordered_map<std::string, AssetKey> m_case_folded_keys;
#endif
    };
}
