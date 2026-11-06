#pragma once

#include "ResourceHandle.h"
#include "ResourceState.h"

#include <Assets/Registry/AssetRegistry.h>
#include <Assets/Formats/StaticAssetFormats.h>
#include <Core/FileSystem/FileMount.h>

#include <cstdint>
#include <deque>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace GE::Resource
{
    class ResourceManager final
    {
    public:
        ResourceManager(const Asset::AssetRegistry& registry, const FileSystem::IReadOnlyFileMount& mount);

        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;

        [[nodiscard]] Core::Result<StaticMeshHandle> RequestStaticMesh(Asset::AssetId id);
        [[nodiscard]] Core::Result<TextureHandle> RequestTexture(Asset::AssetId id);
        [[nodiscard]] Core::Result<MaterialHandle> RequestMaterial(Asset::AssetId id);

        [[nodiscard]] Core::Result<StaticMeshHandle> LoadStaticMesh(Asset::AssetId id);
        [[nodiscard]] Core::Result<TextureHandle> LoadTexture(Asset::AssetId id);
        [[nodiscard]] Core::Result<MaterialHandle> LoadMaterial(Asset::AssetId id);

        [[nodiscard]] Core::Result<void> Load(StaticMeshHandle handle);
        [[nodiscard]] Core::Result<void> Load(TextureHandle handle);
        [[nodiscard]] Core::Result<void> Load(MaterialHandle handle);

        [[nodiscard]] Core::Result<void> Release(StaticMeshHandle handle);
        [[nodiscard]] Core::Result<void> Release(TextureHandle handle);
        [[nodiscard]] Core::Result<void> Release(MaterialHandle handle);

        [[nodiscard]] ResourceState State(StaticMeshHandle handle) const noexcept;
        [[nodiscard]] ResourceState State(TextureHandle handle) const noexcept;
        [[nodiscard]] ResourceState State(MaterialHandle handle) const noexcept;

        [[nodiscard]] std::optional<Core::EngineError> Failure(StaticMeshHandle handle) const noexcept;
        [[nodiscard]] std::optional<Core::EngineError> Failure(TextureHandle handle) const noexcept;
        [[nodiscard]] std::optional<Core::EngineError> Failure(MaterialHandle handle) const noexcept;

        [[nodiscard]] const AssetFormats::StaticMeshAsset& Resolve(StaticMeshHandle handle) const noexcept;
        [[nodiscard]] const AssetFormats::TextureAsset& Resolve(TextureHandle handle) const noexcept;
        [[nodiscard]] const AssetFormats::MaterialAsset& Resolve(MaterialHandle handle) const noexcept;

        [[nodiscard]] const AssetFormats::StaticMeshAsset& FallbackStaticMesh() const noexcept;
        [[nodiscard]] const AssetFormats::TextureAsset& FallbackTexture() const noexcept;
        [[nodiscard]] const AssetFormats::MaterialAsset& FallbackMaterial() const noexcept;

    private:
        template <typename AssetType> struct Slot final
        {
            Asset::AssetId id;
            std::uint32_t generation = 1;
            ResourceState state = ResourceState::Unloaded;
            std::optional<AssetType> asset;
            std::optional<Core::EngineError> failure;
        };

        template <typename AssetType, typename HandleType> struct Pool final
        {
            std::deque<Slot<AssetType>> slots;
            std::vector<std::uint32_t> free_indices;
            std::unordered_map<Asset::AssetId, HandleType> handles;
        };

        template <typename AssetType, typename HandleType>
        [[nodiscard]] Core::Result<HandleType> Request(Asset::AssetId id, Asset::AssetTypeId expected_type,
                                                       Pool<AssetType, HandleType>& pool, std::string_view operation);

        template <typename AssetType, typename HandleType>
        [[nodiscard]] Slot<AssetType>* Find(HandleType handle, Pool<AssetType, HandleType>& pool) noexcept;

        template <typename AssetType, typename HandleType>
        [[nodiscard]] const Slot<AssetType>* Find(HandleType handle,
                                                  const Pool<AssetType, HandleType>& pool) const noexcept;

        template <typename AssetType, typename HandleType>
        [[nodiscard]] Core::Result<void> Release(HandleType handle, Pool<AssetType, HandleType>& pool,
                                                 std::string_view operation);

        template <typename AssetType, typename HandleType>
        [[nodiscard]] ResourceState State(HandleType handle, const Pool<AssetType, HandleType>& pool) const noexcept;

        template <typename AssetType, typename HandleType>
        [[nodiscard]] std::optional<Core::EngineError> Failure(HandleType handle,
                                                               const Pool<AssetType, HandleType>& pool) const noexcept;

        const Asset::AssetRegistry& m_registry;
        const FileSystem::IReadOnlyFileMount& m_mount;
        Pool<AssetFormats::StaticMeshAsset, StaticMeshHandle> m_meshes;
        Pool<AssetFormats::TextureAsset, TextureHandle> m_textures;
        Pool<AssetFormats::MaterialAsset, MaterialHandle> m_materials;
        AssetFormats::StaticMeshAsset m_fallback_mesh;
        AssetFormats::TextureAsset m_fallback_texture;
        AssetFormats::MaterialAsset m_fallback_material;
    };
} // namespace GE::Resource
