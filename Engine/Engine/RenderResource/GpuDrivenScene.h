#pragma once

#include <Engine/Core/Diagnostics/Result.h>
#include <Engine/Core/Math/matrix4.h>
#include <Engine/RHI/RHIDevice.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace GE::RenderResource
{
    struct GpuSceneInstance final
    {
        std::array<float, 3> bounds_center{};
        float bounds_radius = 0.0F;
    };

    struct GpuSceneDraw final
    {
        std::uint32_t index_count = 0;
        std::uint32_t first_index = 0;
        std::int32_t vertex_offset = 0;
    };

    // 最小 GPU Scene：上传包围球，Compute 视锥剔除并生成可见索引和间接参数。
    class GpuDrivenScene final
    {
    public:
        [[nodiscard]] static Core::Result<std::unique_ptr<GpuDrivenScene>> Create(
            RHI::Device& device,
            std::span<const std::byte> compute_shader,
            std::uint32_t max_instances);

        [[nodiscard]] RHI::RHIResult RecordCull(
            RHI::CommandList& commands,
            std::span<const GpuSceneInstance> instances,
            const Math::Mat4& view_projection,
            std::span<const GpuSceneDraw> draws) noexcept;

        [[nodiscard]] RHI::Buffer& IndirectArguments() noexcept;
        [[nodiscard]] RHI::Buffer& VisibleInstanceIndices() noexcept;
        [[nodiscard]] std::uint32_t MaxInstances() const noexcept;

    private:
        GpuDrivenScene(RHI::Device& device, std::uint32_t max_instances) noexcept;

        RHI::Device& m_device;
        std::uint32_t m_max_instances = 0;
        std::unique_ptr<RHI::Buffer> m_instances;
        std::unique_ptr<RHI::Buffer> m_constants;
        std::unique_ptr<RHI::Buffer> m_argument_seed;
        std::unique_ptr<RHI::Buffer> m_arguments;
        std::unique_ptr<RHI::Buffer> m_visible_indices;
        std::unique_ptr<RHI::Shader> m_shader;
        std::unique_ptr<RHI::PipelineLayout> m_layout;
        std::unique_ptr<RHI::ResourceSet> m_resource_set;
        std::unique_ptr<RHI::ComputePipeline> m_pipeline;
    };
}
