#include "ResourceManager.h"

#include <Assets/Cook/CookedContainer.h>

#include <algorithm>
#include <limits>
#include <string>

namespace GE::Resource
{
    namespace
    {
        [[nodiscard]] Core::EngineError ResourceError(Core::ErrorCode code, std::string_view operation,
                                                      std::string message,
                                                      std::optional<std::string> resource = std::nullopt)
        {
            return {.domain = Core::ErrorDomain::Asset,
                    .code = code,
                    .operation = std::string(operation),
                    .message = std::move(message),
                    .resource = std::move(resource)};
        }

        [[nodiscard]] AssetFormats::StaticMeshAsset MakeFallbackMesh()
        {
            AssetFormats::StaticMeshAsset mesh;
            mesh.nodes.push_back({.parent = -1, .local_transform = {}, .first_submesh = 0, .submesh_count = 1});
            mesh.vertices = {{.position = {-0.5F, -0.5F, 0.0F}, .normal = {0.0F, 0.0F, 1.0F}, .texcoord = {0.0F, 1.0F}},
                             {.position = {0.0F, 0.5F, 0.0F}, .normal = {0.0F, 0.0F, 1.0F}, .texcoord = {0.5F, 0.0F}},
                             {.position = {0.5F, -0.5F, 0.0F}, .normal = {0.0F, 0.0F, 1.0F}, .texcoord = {1.0F, 1.0F}}};
            mesh.indices = {0, 1, 2};
            mesh.submeshes.push_back({.first_index = 0,
                                      .index_count = 3,
                                      .material_slot = 0,
                                      .bounds_min = {-0.5F, -0.5F, 0.0F},
                                      .bounds_max = {0.5F, 0.5F, 0.0F}});
            return mesh;
        }

        [[nodiscard]] AssetFormats::TextureAsset MakeFallbackTexture()
        {
            AssetFormats::TextureAsset texture;
            texture.format = AssetFormats::TexturePixelFormat::Rgba8Unorm;
            texture.color_space = AssetFormats::TextureColorSpace::Srgb;
            texture.mips.push_back({.width = 1, .height = 1, .byte_offset = 0, .byte_size = 4});
            texture.pixels = {std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff}};
            return texture;
        }

        template <typename AssetType>
        [[nodiscard]] Core::Result<AssetType> ReadAsset(const FileSystem::IReadOnlyFileMount& mount,
                                                        const Asset::AssetMetadata& metadata,
                                                        Core::Result<AssetType> (*reader)(const Cook::CookedContainer&),
                                                        std::string_view operation)
        {
            if (!metadata.cooked_relative_path.has_value())
            {
                return Core::Result<AssetType>::Failure(ResourceError(
                    Core::ErrorCode::NotFound, operation, "Asset has no cooked path", metadata.key.SourcePath()));
            }
            auto path = FileSystem::NormalizeLogicalPath(*metadata.cooked_relative_path);
            if (!path)
                return Core::Result<AssetType>::Failure(path.Error());
            auto container = Cook::ReadCookedContainer(mount, path.Value());
            if (!container)
                return Core::Result<AssetType>::Failure(container.Error());
            return reader(container.Value());
        }
    } // namespace

    ResourceManager::ResourceManager(const Asset::AssetRegistry& registry, const FileSystem::IReadOnlyFileMount& mount)
        : m_registry(registry), m_mount(mount), m_fallback_mesh(MakeFallbackMesh()),
          m_fallback_texture(MakeFallbackTexture())
    {
    }

    template <typename AssetType, typename HandleType>
    Core::Result<HandleType> ResourceManager::Request(Asset::AssetId id, Asset::AssetTypeId expected_type,
                                                      Pool<AssetType, HandleType>& pool, std::string_view operation)
    {
        if (!m_registry.IsFrozen())
        {
            return Core::Result<HandleType>::Failure(ResourceError(
                Core::ErrorCode::InvalidState, operation, "Resource manager requires a frozen asset registry"));
        }
        const Asset::AssetMetadata* metadata = m_registry.Find(id);
        if (metadata == nullptr)
        {
            return Core::Result<HandleType>::Failure(
                ResourceError(Core::ErrorCode::NotFound, operation, "AssetId is not registered", Asset::ToString(id)));
        }
        if (metadata->type_id != expected_type)
        {
            return Core::Result<HandleType>::Failure(
                ResourceError(Core::ErrorCode::InvalidArgument, operation,
                              "Asset type does not match the requested resource type", metadata->key.SourcePath()));
        }
        if (!metadata->cooked_relative_path.has_value())
        {
            return Core::Result<HandleType>::Failure(ResourceError(
                Core::ErrorCode::NotFound, operation, "Asset has no cooked path", metadata->key.SourcePath()));
        }

        const auto existing = pool.handles.find(id);
        if (existing != pool.handles.end())
            return Core::Result<HandleType>::Success(existing->second);

        std::uint32_t index = 0;
        if (!pool.free_indices.empty())
        {
            index = pool.free_indices.back();
            pool.free_indices.pop_back();
        }
        else
        {
            if (pool.slots.size() >= std::numeric_limits<std::uint32_t>::max())
            {
                return Core::Result<HandleType>::Failure(
                    ResourceError(Core::ErrorCode::InvalidState, operation, "Resource handle space is exhausted"));
            }
            index = static_cast<std::uint32_t>(pool.slots.size());
            pool.slots.emplace_back();
        }

        Slot<AssetType>& slot = pool.slots[index];
        slot.id = id;
        slot.state = ResourceState::Unloaded;
        slot.asset.reset();
        slot.failure.reset();
        const HandleType handle(index, slot.generation);
        pool.handles.emplace(id, handle);
        return Core::Result<HandleType>::Success(handle);
    }

    template <typename AssetType, typename HandleType>
    ResourceManager::Slot<AssetType>* ResourceManager::Find(HandleType handle,
                                                            Pool<AssetType, HandleType>& pool) noexcept
    {
        if (!handle.IsValid() || handle.Index() >= pool.slots.size())
            return nullptr;
        Slot<AssetType>& slot = pool.slots[handle.Index()];
        return slot.generation == handle.Generation() && slot.id.IsValid() ? &slot : nullptr;
    }

    template <typename AssetType, typename HandleType>
    const ResourceManager::Slot<AssetType>* ResourceManager::Find(
        HandleType handle, const Pool<AssetType, HandleType>& pool) const noexcept
    {
        if (!handle.IsValid() || handle.Index() >= pool.slots.size())
            return nullptr;
        const Slot<AssetType>& slot = pool.slots[handle.Index()];
        return slot.generation == handle.Generation() && slot.id.IsValid() ? &slot : nullptr;
    }

    template <typename AssetType, typename HandleType>
    Core::Result<void> ResourceManager::Release(HandleType handle, Pool<AssetType, HandleType>& pool,
                                                std::string_view operation)
    {
        Slot<AssetType>* slot = Find(handle, pool);
        if (slot == nullptr)
        {
            return Core::Result<void>::Failure(
                ResourceError(Core::ErrorCode::InvalidArgument, operation, "Resource handle is stale or invalid"));
        }
        pool.handles.erase(slot->id);
        slot->id = {};
        slot->state = ResourceState::Unloaded;
        slot->asset.reset();
        slot->failure.reset();
        ++slot->generation;
        if (slot->generation == 0)
            slot->generation = 1;
        pool.free_indices.push_back(handle.Index());
        return Core::Result<void>::Success();
    }

    template <typename AssetType, typename HandleType>
    ResourceState ResourceManager::State(HandleType handle, const Pool<AssetType, HandleType>& pool) const noexcept
    {
        const Slot<AssetType>* slot = Find(handle, pool);
        return slot == nullptr ? ResourceState::Unloaded : slot->state;
    }

    template <typename AssetType, typename HandleType>
    std::optional<Core::EngineError> ResourceManager::Failure(HandleType handle,
                                                              const Pool<AssetType, HandleType>& pool) const noexcept
    {
        const Slot<AssetType>* slot = Find(handle, pool);
        return slot != nullptr && slot->failure.has_value() ? slot->failure : std::nullopt;
    }

    Core::Result<StaticMeshHandle> ResourceManager::RequestStaticMesh(Asset::AssetId id)
    {
        return Request(id, Asset::MakeAssetTypeId(AssetFormats::StaticMeshTypeName), m_meshes, "RequestStaticMesh");
    }

    Core::Result<TextureHandle> ResourceManager::RequestTexture(Asset::AssetId id)
    {
        return Request(id, Asset::MakeAssetTypeId(AssetFormats::TextureTypeName), m_textures, "RequestTexture");
    }

    Core::Result<MaterialHandle> ResourceManager::RequestMaterial(Asset::AssetId id)
    {
        return Request(id, Asset::MakeAssetTypeId(AssetFormats::MaterialTypeName), m_materials, "RequestMaterial");
    }

    Core::Result<StaticMeshHandle> ResourceManager::LoadStaticMesh(Asset::AssetId id)
    {
        auto handle = RequestStaticMesh(id);
        if (!handle)
            return handle;
        auto loaded = Load(handle.Value());
        return loaded ? Core::Result<StaticMeshHandle>::Success(handle.Value())
                      : Core::Result<StaticMeshHandle>::Failure(loaded.Error());
    }

    Core::Result<TextureHandle> ResourceManager::LoadTexture(Asset::AssetId id)
    {
        auto handle = RequestTexture(id);
        if (!handle)
            return handle;
        auto loaded = Load(handle.Value());
        return loaded ? Core::Result<TextureHandle>::Success(handle.Value())
                      : Core::Result<TextureHandle>::Failure(loaded.Error());
    }

    Core::Result<MaterialHandle> ResourceManager::LoadMaterial(Asset::AssetId id)
    {
        auto handle = RequestMaterial(id);
        if (!handle)
            return handle;
        auto loaded = Load(handle.Value());
        return loaded ? Core::Result<MaterialHandle>::Success(handle.Value())
                      : Core::Result<MaterialHandle>::Failure(loaded.Error());
    }

    Core::Result<void> ResourceManager::Load(StaticMeshHandle handle)
    {
        Slot<AssetFormats::StaticMeshAsset>* slot = Find(handle, m_meshes);
        if (slot == nullptr)
            return Core::Result<void>::Failure(ResourceError(Core::ErrorCode::InvalidArgument, "LoadStaticMesh",
                                                             "Resource handle is stale or invalid"));
        if (slot->state == ResourceState::Ready)
            return Core::Result<void>::Success();
        if (slot->state == ResourceState::Failed)
            return Core::Result<void>::Failure(*slot->failure);
        if (slot->state == ResourceState::Loading)
            return Core::Result<void>::Failure(
                ResourceError(Core::ErrorCode::InvalidState, "LoadStaticMesh", "Recursive resource load detected"));

        slot->state = ResourceState::Loading;
        auto loaded = ReadAsset(m_mount, *m_registry.Find(slot->id), &AssetFormats::ReadStaticMesh, "LoadStaticMesh");
        if (!loaded)
        {
            slot->failure = loaded.Error();
            slot->state = ResourceState::Failed;
            return Core::Result<void>::Failure(loaded.Error());
        }
        slot->asset = std::move(loaded).Value();
        slot->state = ResourceState::Ready;
        return Core::Result<void>::Success();
    }

    Core::Result<void> ResourceManager::Load(TextureHandle handle)
    {
        Slot<AssetFormats::TextureAsset>* slot = Find(handle, m_textures);
        if (slot == nullptr)
            return Core::Result<void>::Failure(
                ResourceError(Core::ErrorCode::InvalidArgument, "LoadTexture", "Resource handle is stale or invalid"));
        if (slot->state == ResourceState::Ready)
            return Core::Result<void>::Success();
        if (slot->state == ResourceState::Failed)
            return Core::Result<void>::Failure(*slot->failure);
        if (slot->state == ResourceState::Loading)
            return Core::Result<void>::Failure(
                ResourceError(Core::ErrorCode::InvalidState, "LoadTexture", "Recursive resource load detected"));

        slot->state = ResourceState::Loading;
        auto loaded = ReadAsset(m_mount, *m_registry.Find(slot->id), &AssetFormats::ReadTexture, "LoadTexture");
        if (!loaded)
        {
            slot->failure = loaded.Error();
            slot->state = ResourceState::Failed;
            return Core::Result<void>::Failure(loaded.Error());
        }
        slot->asset = std::move(loaded).Value();
        slot->state = ResourceState::Ready;
        return Core::Result<void>::Success();
    }

    Core::Result<void> ResourceManager::Load(MaterialHandle handle)
    {
        Slot<AssetFormats::MaterialAsset>* slot = Find(handle, m_materials);
        if (slot == nullptr)
            return Core::Result<void>::Failure(
                ResourceError(Core::ErrorCode::InvalidArgument, "LoadMaterial", "Resource handle is stale or invalid"));
        if (slot->state == ResourceState::Ready)
            return Core::Result<void>::Success();
        if (slot->state == ResourceState::Failed)
            return Core::Result<void>::Failure(*slot->failure);
        if (slot->state == ResourceState::Loading)
            return Core::Result<void>::Failure(
                ResourceError(Core::ErrorCode::InvalidState, "LoadMaterial", "Recursive resource load detected"));

        slot->state = ResourceState::Loading;
        const Asset::AssetMetadata& metadata = *m_registry.Find(slot->id);
        for (const Asset::AssetId dependency : metadata.dependencies)
        {
            const Asset::AssetMetadata* dependency_metadata = m_registry.Find(dependency);
            if (dependency_metadata == nullptr ||
                dependency_metadata->type_id != Asset::MakeAssetTypeId(AssetFormats::TextureTypeName))
            {
                slot->failure = ResourceError(Core::ErrorCode::InvalidArgument, "LoadMaterial",
                                              "Material dependency is not a Texture2D", metadata.key.SourcePath());
                slot->state = ResourceState::Failed;
                return Core::Result<void>::Failure(*slot->failure);
            }
            auto dependency_loaded = LoadTexture(dependency);
            if (!dependency_loaded)
            {
                slot->failure = dependency_loaded.Error();
                slot->state = ResourceState::Failed;
                return Core::Result<void>::Failure(*slot->failure);
            }
        }

        auto loaded = ReadAsset(m_mount, metadata, &AssetFormats::ReadMaterial, "LoadMaterial");
        if (!loaded)
        {
            slot->failure = loaded.Error();
            slot->state = ResourceState::Failed;
            return Core::Result<void>::Failure(loaded.Error());
        }
        const AssetFormats::MaterialAsset& material = loaded.Value();
        const Asset::AssetId texture_ids[] = {material.base_color_texture, material.metallic_roughness_texture,
                                              material.normal_texture, material.emissive_texture};
        for (const Asset::AssetId texture_id : texture_ids)
        {
            if (!texture_id.IsValid())
                continue;
            if (std::find(metadata.dependencies.begin(), metadata.dependencies.end(), texture_id) ==
                metadata.dependencies.end())
            {
                slot->failure =
                    ResourceError(Core::ErrorCode::InvalidArgument, "LoadMaterial",
                                  "Cooked material references an undeclared dependency", metadata.key.SourcePath());
                slot->state = ResourceState::Failed;
                return Core::Result<void>::Failure(*slot->failure);
            }
        }
        slot->asset = std::move(loaded).Value();
        slot->state = ResourceState::Ready;
        return Core::Result<void>::Success();
    }

    Core::Result<void> ResourceManager::Release(StaticMeshHandle handle)
    {
        return Release(handle, m_meshes, "ReleaseStaticMesh");
    }
    Core::Result<void> ResourceManager::Release(TextureHandle handle)
    {
        return Release(handle, m_textures, "ReleaseTexture");
    }
    Core::Result<void> ResourceManager::Release(MaterialHandle handle)
    {
        return Release(handle, m_materials, "ReleaseMaterial");
    }

    ResourceState ResourceManager::State(StaticMeshHandle handle) const noexcept
    {
        return State(handle, m_meshes);
    }
    ResourceState ResourceManager::State(TextureHandle handle) const noexcept
    {
        return State(handle, m_textures);
    }
    ResourceState ResourceManager::State(MaterialHandle handle) const noexcept
    {
        return State(handle, m_materials);
    }

    std::optional<Core::EngineError> ResourceManager::Failure(StaticMeshHandle handle) const noexcept
    {
        return Failure(handle, m_meshes);
    }
    std::optional<Core::EngineError> ResourceManager::Failure(TextureHandle handle) const noexcept
    {
        return Failure(handle, m_textures);
    }
    std::optional<Core::EngineError> ResourceManager::Failure(MaterialHandle handle) const noexcept
    {
        return Failure(handle, m_materials);
    }

    const AssetFormats::StaticMeshAsset& ResourceManager::Resolve(StaticMeshHandle handle) const noexcept
    {
        const auto* slot = Find(handle, m_meshes);
        return slot != nullptr && slot->state == ResourceState::Ready && slot->asset.has_value() ? *slot->asset
                                                                                                 : m_fallback_mesh;
    }
    const AssetFormats::TextureAsset& ResourceManager::Resolve(TextureHandle handle) const noexcept
    {
        const auto* slot = Find(handle, m_textures);
        return slot != nullptr && slot->state == ResourceState::Ready && slot->asset.has_value() ? *slot->asset
                                                                                                 : m_fallback_texture;
    }
    const AssetFormats::MaterialAsset& ResourceManager::Resolve(MaterialHandle handle) const noexcept
    {
        const auto* slot = Find(handle, m_materials);
        return slot != nullptr && slot->state == ResourceState::Ready && slot->asset.has_value() ? *slot->asset
                                                                                                 : m_fallback_material;
    }

    const AssetFormats::StaticMeshAsset& ResourceManager::FallbackStaticMesh() const noexcept
    {
        return m_fallback_mesh;
    }
    const AssetFormats::TextureAsset& ResourceManager::FallbackTexture() const noexcept
    {
        return m_fallback_texture;
    }
    const AssetFormats::MaterialAsset& ResourceManager::FallbackMaterial() const noexcept
    {
        return m_fallback_material;
    }
} // namespace GE::Resource
