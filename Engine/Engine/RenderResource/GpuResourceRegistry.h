#pragma once

#include "RenderResourceUploader.h"

#include <Engine/Core/Diagnostics/Result.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <vector>

namespace GE::RenderResource
{
    template<typename Tag>
    struct GpuHandle final
    {
        std::uint32_t index = 0;
        std::uint32_t generation = 0;
        [[nodiscard]] explicit operator bool() const noexcept
        {
            return generation != 0;
        }
        bool operator==(const GpuHandle&) const noexcept = default;
    };

    struct GpuMeshTag;
    struct GpuTextureTag;
    using GpuMeshHandle = GpuHandle<GpuMeshTag>;
    using GpuTextureHandle = GpuHandle<GpuTextureTag>;

    // 句柄在 Release 时立刻失效；底层对象等 GPU Fence 完成后才真正析构。
    class GpuResourceRegistry final
    {
    public:
        [[nodiscard]] GpuMeshHandle Publish(GpuMeshResource resource);
        [[nodiscard]] GpuTextureHandle Publish(GpuTextureResource resource);
        [[nodiscard]] GpuMeshHandle InstallFallback(GpuMeshResource resource);
        [[nodiscard]] GpuTextureHandle InstallFallback(GpuTextureResource resource);

        [[nodiscard]] const GpuMeshResource* Resolve(
            GpuMeshHandle handle) const noexcept;
        [[nodiscard]] const GpuTextureResource* Resolve(
            GpuTextureHandle handle) const noexcept;

        [[nodiscard]] Core::Result<void> Release(
            GpuMeshHandle handle, std::uint64_t retire_fence_value);
        [[nodiscard]] Core::Result<void> Release(
            GpuTextureHandle handle, std::uint64_t retire_fence_value);
        void Collect(std::uint64_t completed_fence_value) noexcept;
        [[nodiscard]] std::size_t PendingRetirementCount() const noexcept;

    private:
        template<typename Resource>
        struct Slot final
        {
            std::uint32_t generation = 1;
            std::unique_ptr<Resource> resource;
            bool fallback = false;
        };

        template<typename Resource>
        struct Retired final
        {
            std::uint64_t fence_value = 0;
            std::unique_ptr<Resource> resource;
        };

        std::deque<Slot<GpuMeshResource>> m_meshes;
        std::deque<Slot<GpuTextureResource>> m_textures;
        std::vector<std::uint32_t> m_free_meshes;
        std::vector<std::uint32_t> m_free_textures;
        std::deque<Retired<GpuMeshResource>> m_retired_meshes;
        std::deque<Retired<GpuTextureResource>> m_retired_textures;
        GpuMeshHandle m_fallback_mesh;
        GpuTextureHandle m_fallback_texture;
    };
}
