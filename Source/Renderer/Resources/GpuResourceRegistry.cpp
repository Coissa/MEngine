#include "GpuResourceRegistry.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

namespace GE::RenderResource
{
    namespace
    {
        [[nodiscard]] Core::EngineError InvalidHandle(std::string operation)
        {
            return {.domain = Core::ErrorDomain::Core,
                    .code = Core::ErrorCode::InvalidArgument,
                    .operation = std::move(operation),
                    .message = "GPU resource handle is stale, invalid, or protected"};
        }

        template <typename Resource, typename Handle, typename Slots>
        [[nodiscard]] Handle PublishTo(Resource resource, Slots& slots, std::vector<std::uint32_t>& free_indices)
        {
            std::uint32_t index = 0;
            if (free_indices.empty())
            {
                index = static_cast<std::uint32_t>(slots.size());
                slots.emplace_back();
            }
            else
            {
                index = free_indices.back();
                free_indices.pop_back();
            }
            auto& slot = slots[index];
            slot.resource = std::make_unique<Resource>(std::move(resource));
            slot.fallback = false;
            return {index, slot.generation};
        }

        template <typename Resource, typename Handle, typename Slots>
        [[nodiscard]] const Resource* Find(Handle handle, const Slots& slots) noexcept
        {
            if (!handle || handle.index >= slots.size())
                return nullptr;
            const auto& slot = slots[handle.index];
            return slot.generation == handle.generation ? slot.resource.get() : nullptr;
        }

        template <typename Resource, typename Handle, typename Slots, typename Retired>
        [[nodiscard]] Core::Result<void> ReleaseFrom(Handle handle, std::uint64_t fence_value, Slots& slots,
                                                     std::vector<std::uint32_t>& free_indices, Retired& retired,
                                                     std::string operation)
        {
            if (!handle || handle.index >= slots.size())
                return Core::Result<void>::Failure(InvalidHandle(std::move(operation)));
            auto& slot = slots[handle.index];
            if (slot.generation != handle.generation || !slot.resource || slot.fallback)
                return Core::Result<void>::Failure(InvalidHandle(std::move(operation)));
            retired.push_back({fence_value, std::move(slot.resource)});
            slot.generation = slot.generation == std::numeric_limits<std::uint32_t>::max() ? 1u : slot.generation + 1u;
            free_indices.push_back(handle.index);
            return Core::Result<void>::Success();
        }

        template <typename Retired> void CollectFrom(std::uint64_t completed, Retired& retired) noexcept
        {
            std::erase_if(retired, [completed](const auto& value) { return value.fence_value <= completed; });
        }
    } // namespace

    GpuMeshHandle GpuResourceRegistry::Publish(GpuMeshResource resource)
    {
        return PublishTo<GpuMeshResource, GpuMeshHandle>(std::move(resource), m_meshes, m_free_meshes);
    }

    GpuTextureHandle GpuResourceRegistry::Publish(GpuTextureResource resource)
    {
        return PublishTo<GpuTextureResource, GpuTextureHandle>(std::move(resource), m_textures, m_free_textures);
    }

    GpuMeshHandle GpuResourceRegistry::InstallFallback(GpuMeshResource resource)
    {
        if (Resolve(m_fallback_mesh) != nullptr)
            return m_fallback_mesh;
        m_fallback_mesh = Publish(std::move(resource));
        m_meshes[m_fallback_mesh.index].fallback = true;
        return m_fallback_mesh;
    }

    GpuTextureHandle GpuResourceRegistry::InstallFallback(GpuTextureResource resource)
    {
        if (Resolve(m_fallback_texture) != nullptr)
            return m_fallback_texture;
        m_fallback_texture = Publish(std::move(resource));
        m_textures[m_fallback_texture.index].fallback = true;
        return m_fallback_texture;
    }

    const GpuMeshResource* GpuResourceRegistry::Resolve(GpuMeshHandle handle) const noexcept
    {
        if (const auto* resource = Find<GpuMeshResource>(handle, m_meshes))
            return resource;
        return Find<GpuMeshResource>(m_fallback_mesh, m_meshes);
    }

    const GpuTextureResource* GpuResourceRegistry::Resolve(GpuTextureHandle handle) const noexcept
    {
        if (const auto* resource = Find<GpuTextureResource>(handle, m_textures))
            return resource;
        return Find<GpuTextureResource>(m_fallback_texture, m_textures);
    }

    Core::Result<void> GpuResourceRegistry::Release(GpuMeshHandle handle, std::uint64_t retire_fence_value)
    {
        return ReleaseFrom<GpuMeshResource>(handle, retire_fence_value, m_meshes, m_free_meshes, m_retired_meshes,
                                            "GpuResourceRegistry.ReleaseMesh");
    }

    Core::Result<void> GpuResourceRegistry::Release(GpuTextureHandle handle, std::uint64_t retire_fence_value)
    {
        return ReleaseFrom<GpuTextureResource>(handle, retire_fence_value, m_textures, m_free_textures,
                                               m_retired_textures, "GpuResourceRegistry.ReleaseTexture");
    }

    void GpuResourceRegistry::Collect(std::uint64_t completed_fence_value) noexcept
    {
        CollectFrom(completed_fence_value, m_retired_meshes);
        CollectFrom(completed_fence_value, m_retired_textures);
    }

    std::size_t GpuResourceRegistry::PendingRetirementCount() const noexcept
    {
        return m_retired_meshes.size() + m_retired_textures.size();
    }
} // namespace GE::RenderResource
