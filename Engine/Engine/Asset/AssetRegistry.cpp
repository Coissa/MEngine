#include "AssetRegistry.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace GE::Asset
{
    namespace
    {
        [[nodiscard]] Core::EngineError AssetError(
            Core::ErrorCode code,
            std::string operation,
            std::string message,
            std::optional<std::string> resource = std::nullopt)
        {
            return {
                .domain = Core::ErrorDomain::Asset,
                .code = code,
                .operation = std::move(operation),
                .message = std::move(message),
                .resource = std::move(resource)};
        }

        [[nodiscard]] bool IsStableTypeName(std::string_view name) noexcept
        {
            if (name.empty()) return false;
            return std::all_of(name.begin(), name.end(), [](unsigned char value)
            {
                return std::isalnum(value) != 0 || value == '.'
                    || value == '_' || value == '-';
            });
        }

#ifdef _WIN32
        [[nodiscard]] std::string FoldKey(const AssetKey& key)
        {
            std::string result = key.SourcePath();
            result.push_back('\n');
            result += key.SubassetKey();
            std::transform(result.begin(), result.end(), result.begin(),
                [](unsigned char value)
                {
                    return static_cast<char>(std::tolower(value));
                });
            return result;
        }
#endif
    }

    AssetRegistry::AssetRegistry(AssetId project_namespace) noexcept
        : m_project_namespace(project_namespace)
    {
    }

    Core::Result<AssetId> AssetRegistry::Add(AssetRecord record)
    {
        if (m_frozen)
        {
            return Core::Result<AssetId>::Failure(AssetError(
                Core::ErrorCode::InvalidState, "AssetRegistry.Add",
                "Cannot add an asset after the registry is frozen"));
        }
        if (!m_project_namespace.IsValid() || !record.key.IsValid()
            || !IsStableTypeName(record.stable_type_name))
        {
            return Core::Result<AssetId>::Failure(AssetError(
                Core::ErrorCode::InvalidArgument, "AssetRegistry.Add",
                "Asset record has an invalid namespace, key, or type name"));
        }

        if (record.cooked_relative_path.has_value())
        {
            auto normalized = NormalizeAssetRelativePath(
                *record.cooked_relative_path);
            if (!normalized)
            {
                return Core::Result<AssetId>::Failure(normalized.Error());
            }
            record.cooked_relative_path = std::move(normalized).Value();
        }
        for (const AssetKey& dependency : record.dependencies)
        {
            if (!dependency.IsValid())
            {
                return Core::Result<AssetId>::Failure(AssetError(
                    Core::ErrorCode::InvalidArgument, "AssetRegistry.Add",
                    "Asset dependency has an invalid key"));
            }
        }

        if (m_key_indices.contains(record.key))
        {
            return Core::Result<AssetId>::Failure(AssetError(
                Core::ErrorCode::Conflict, "AssetRegistry.Add",
                "Duplicate asset key", record.key.SourcePath()));
        }
#ifdef _WIN32
        const std::string folded = FoldKey(record.key);
        const auto folded_it = m_case_folded_keys.find(folded);
        if (folded_it != m_case_folded_keys.end()
            && folded_it->second != record.key)
        {
            return Core::Result<AssetId>::Failure(AssetError(
                Core::ErrorCode::Conflict, "AssetRegistry.Add",
                "Asset keys differ only by case", record.key.SourcePath()));
        }
#endif

        const AssetTypeId type_id = MakeAssetTypeId(record.stable_type_name);
        const auto type_it = m_type_names.find(type_id);
        if (type_it != m_type_names.end()
            && type_it->second != record.stable_type_name)
        {
            return Core::Result<AssetId>::Failure(AssetError(
                Core::ErrorCode::Conflict, "AssetRegistry.Add",
                "Asset type name hash collision", record.stable_type_name));
        }

        const AssetId id = MakeAssetIdV5(
            m_project_namespace, MakeAssetIdName(record.key));
        if (!id.IsValid() || m_id_indices.contains(id))
        {
            return Core::Result<AssetId>::Failure(AssetError(
                Core::ErrorCode::Conflict, "AssetRegistry.Add",
                "Derived AssetId is invalid or already registered",
                record.key.SourcePath()));
        }

        const std::size_t index = m_pending.size();
        m_key_indices.emplace(record.key, index);
        m_id_indices.emplace(id, index);
        m_type_names.emplace(type_id, record.stable_type_name);
#ifdef _WIN32
        m_case_folded_keys.emplace(folded, record.key);
#endif
        m_pending.push_back({id, type_id, std::move(record)});
        return Core::Result<AssetId>::Success(id);
    }

    Core::Result<void> AssetRegistry::Freeze()
    {
        if (m_frozen)
        {
            return Core::Result<void>::Failure(AssetError(
                Core::ErrorCode::InvalidState, "AssetRegistry.Freeze",
                "Asset registry is already frozen"));
        }

        std::sort(m_pending.begin(), m_pending.end(),
            [](const PendingEntry& left, const PendingEntry& right)
            {
                return left.record.key < right.record.key;
            });

        m_id_indices.clear();
        m_key_indices.clear();
        for (std::size_t index = 0; index < m_pending.size(); ++index)
        {
            m_id_indices.emplace(m_pending[index].id, index);
            m_key_indices.emplace(m_pending[index].record.key, index);
        }

        m_metadata.clear();
        m_metadata.reserve(m_pending.size());
        m_referencers.assign(m_pending.size(), {});
        for (const PendingEntry& entry : m_pending)
        {
            std::vector<AssetId> dependencies;
            dependencies.reserve(entry.record.dependencies.size());
            std::unordered_set<AssetId> unique_dependencies;
            for (const AssetKey& dependency_key : entry.record.dependencies)
            {
                const auto dependency_it = m_key_indices.find(dependency_key);
                if (dependency_it == m_key_indices.end())
                {
                    return Core::Result<void>::Failure(AssetError(
                        Core::ErrorCode::NotFound, "AssetRegistry.Freeze",
                        "Asset dependency is not registered",
                        dependency_key.SourcePath()));
                }
                const AssetId dependency_id =
                    m_pending[dependency_it->second].id;
                if (!unique_dependencies.insert(dependency_id).second)
                {
                    return Core::Result<void>::Failure(AssetError(
                        Core::ErrorCode::Conflict, "AssetRegistry.Freeze",
                        "Asset dependency is listed more than once",
                        dependency_key.SourcePath()));
                }
                dependencies.push_back(dependency_id);
            }
            std::sort(dependencies.begin(), dependencies.end());
            m_metadata.push_back({
                .id = entry.id,
                .key = entry.record.key,
                .type_id = entry.type_id,
                .stable_type_name = entry.record.stable_type_name,
                .cooked_relative_path = entry.record.cooked_relative_path,
                .importer_version = entry.record.importer_version,
                .cooked_version = entry.record.cooked_version,
                .dependencies = std::move(dependencies)});
        }

        for (std::size_t source_index = 0;
             source_index < m_metadata.size(); ++source_index)
        {
            for (const AssetId dependency :
                 m_metadata[source_index].dependencies)
            {
                m_referencers[m_id_indices.at(dependency)].push_back(
                    m_metadata[source_index].id);
            }
        }
        for (auto& referencers : m_referencers)
        {
            std::sort(referencers.begin(), referencers.end());
        }

        m_pending.clear();
        m_pending.shrink_to_fit();
#ifdef _WIN32
        m_case_folded_keys.clear();
        m_case_folded_keys.rehash(0);
#endif
        m_frozen = true;
        return Core::Result<void>::Success();
    }

    bool AssetRegistry::IsFrozen() const noexcept { return m_frozen; }
    AssetId AssetRegistry::GetProjectNamespace() const noexcept
    {
        return m_project_namespace;
    }

    const AssetMetadata* AssetRegistry::Find(AssetId id) const noexcept
    {
        if (!m_frozen) return nullptr;
        const auto it = m_id_indices.find(id);
        return it == m_id_indices.end() ? nullptr : &m_metadata[it->second];
    }

    const AssetMetadata* AssetRegistry::Find(
        const AssetKey& key) const noexcept
    {
        if (!m_frozen) return nullptr;
        const auto it = m_key_indices.find(key);
        return it == m_key_indices.end() ? nullptr : &m_metadata[it->second];
    }

    std::span<const AssetMetadata> AssetRegistry::GetAll() const noexcept
    {
        return m_frozen ? std::span<const AssetMetadata>(m_metadata)
                        : std::span<const AssetMetadata>();
    }

    std::span<const AssetId> AssetRegistry::GetDependencies(
        AssetId id) const noexcept
    {
        const AssetMetadata* metadata = Find(id);
        return metadata == nullptr
            ? std::span<const AssetId>()
            : std::span<const AssetId>(metadata->dependencies);
    }

    std::span<const AssetId> AssetRegistry::GetReferencers(
        AssetId id) const noexcept
    {
        if (!m_frozen) return {};
        const auto it = m_id_indices.find(id);
        return it == m_id_indices.end()
            ? std::span<const AssetId>()
            : std::span<const AssetId>(m_referencers[it->second]);
    }
}
