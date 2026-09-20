#include "GpuDrivenScene.h"

#include <Engine/RHI/RHICommand.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

namespace GE::RenderResource
{
    namespace
    {
        struct alignas(16) CullingConstants final
        {
            std::array<std::array<float, 4>, 6> planes{};
            std::uint32_t instance_count = 0;
            std::array<std::uint32_t, 3> padding{};
        };

        [[nodiscard]] Core::EngineError MakeError(
            std::string message, RHI::RHIResult result)
        {
            return {
                .domain = Core::ErrorDomain::Core,
                .code = result == RHI::RHIResult::InvalidArgument
                    ? Core::ErrorCode::InvalidArgument
                    : Core::ErrorCode::InvalidState,
                .operation = "GpuDrivenScene.Create",
                .message = std::move(message),
                .native_code = static_cast<std::int64_t>(result)};
        }

        [[nodiscard]] std::array<float, 4> NormalizePlane(
            std::array<float, 4> plane) noexcept
        {
            const float length = std::sqrt(
                plane[0] * plane[0] + plane[1] * plane[1]
                + plane[2] * plane[2]);
            if (length > 0.0F)
                for (float& value : plane) value /= length;
            return plane;
        }

        [[nodiscard]] CullingConstants BuildConstants(
            const Math::Mat4& matrix, std::uint32_t count) noexcept
        {
            const auto column = [&matrix](std::size_t index)
            {
                return std::array<float, 4>{
                    matrix.At(0, index), matrix.At(1, index),
                    matrix.At(2, index), matrix.At(3, index)};
            };
            const auto add = [](const auto& left, const auto& right, float sign)
            {
                return NormalizePlane({
                    left[0] + sign * right[0], left[1] + sign * right[1],
                    left[2] + sign * right[2], left[3] + sign * right[3]});
            };
            const auto x = column(0);
            const auto y = column(1);
            const auto z = column(2);
            const auto w = column(3);
            CullingConstants result;
            result.planes = {add(w, x, 1.0F), add(w, x, -1.0F),
                add(w, y, 1.0F), add(w, y, -1.0F),
                NormalizePlane(z), add(w, z, -1.0F)};
            result.instance_count = count;
            return result;
        }
    }

    GpuDrivenScene::GpuDrivenScene(
        RHI::Device& device, std::uint32_t max_instances) noexcept
        : m_device(device), m_max_instances(max_instances)
    {
    }

    Core::Result<std::unique_ptr<GpuDrivenScene>> GpuDrivenScene::Create(
        RHI::Device& device,
        std::span<const std::byte> compute_shader,
        std::uint32_t max_instances)
    {
        if (compute_shader.empty() || max_instances == 0
            || max_instances > std::numeric_limits<std::uint32_t>::max()
                / sizeof(GpuSceneInstance))
            return Core::Result<std::unique_ptr<GpuDrivenScene>>::Failure(
                MakeError("Invalid shader or capacity", RHI::RHIResult::InvalidArgument));

        auto scene = std::unique_ptr<GpuDrivenScene>(
            new GpuDrivenScene(device, max_instances));
        RHI::RHIResult result = device.CreateBuffer(
            {.size = static_cast<std::uint64_t>(max_instances)
                    * sizeof(GpuSceneInstance),
             .usage = RHI::BufferUsage::Storage,
             .memory = RHI::MemoryType::Upload,
             .debug_name = "GPU Scene instances"},
            RHI::BufferState::ShaderRead, scene->m_instances);
        if (result == RHI::RHIResult::Success)
            result = device.CreateBuffer(
                {.size = sizeof(CullingConstants),
                 .usage = RHI::BufferUsage::Constant,
                 .memory = RHI::MemoryType::Upload,
                 .debug_name = "GPU Scene culling constants"},
                RHI::BufferState::Constant, scene->m_constants);
        if (result == RHI::RHIResult::Success)
            result = device.CreateBuffer(
                {.size = static_cast<std::uint64_t>(max_instances)
                    * sizeof(RHI::DrawIndexedIndirectArguments),
                 .usage = RHI::BufferUsage::CopySource,
                 .memory = RHI::MemoryType::Upload,
                 .debug_name = "GPU Scene argument seed"},
                RHI::BufferState::CopySource, scene->m_argument_seed);
        if (result == RHI::RHIResult::Success)
            result = device.CreateBuffer(
                {.size = static_cast<std::uint64_t>(max_instances)
                    * sizeof(RHI::DrawIndexedIndirectArguments),
                 .usage = RHI::BufferUsage::Storage | RHI::BufferUsage::Indirect
                    | RHI::BufferUsage::CopyDestination,
                 .memory = RHI::MemoryType::DeviceLocal,
                 .debug_name = "GPU Scene indirect arguments"},
                RHI::BufferState::Indirect, scene->m_arguments);
        if (result == RHI::RHIResult::Success)
            result = device.CreateBuffer(
                {.size = static_cast<std::uint64_t>(max_instances)
                    * sizeof(std::uint32_t),
                 .usage = RHI::BufferUsage::Storage,
                 .memory = RHI::MemoryType::DeviceLocal,
                 .debug_name = "GPU Scene visible instance indices"},
                RHI::BufferState::Storage, scene->m_visible_indices);
        if (result == RHI::RHIResult::Success)
            result = device.CreateShader(
                {.stage = RHI::ShaderStage::Compute,
                 .format = RHI::ShaderBytecodeFormat::D3DBytecode,
                 .bytecode = compute_shader,
                 .debug_name = "GPU Scene frustum culling"}, scene->m_shader);
        const std::array<RHI::ResourceBindingLayout, 4> bindings{{
            {.binding = 0, .type = RHI::ResourceBindingType::ConstantBuffer,
             .visibility = RHI::ShaderVisibility::Compute},
            {.binding = 1, .type = RHI::ResourceBindingType::ReadOnlyStorageBuffer,
             .visibility = RHI::ShaderVisibility::Compute},
            {.binding = 2, .type = RHI::ResourceBindingType::ReadWriteStorageBuffer,
             .visibility = RHI::ShaderVisibility::Compute},
            {.binding = 3, .type = RHI::ResourceBindingType::ReadWriteStorageBuffer,
             .visibility = RHI::ShaderVisibility::Compute}}};
        if (result == RHI::RHIResult::Success)
            result = device.CreatePipelineLayout({.bindings = bindings}, scene->m_layout);
        if (result == RHI::RHIResult::Success)
            result = device.CreateComputePipeline(
                {.layout = scene->m_layout.get(), .compute_shader = scene->m_shader.get()},
                scene->m_pipeline);
        const std::array<RHI::BufferBinding, 4> resources{{
            {.binding = 0, .buffer = scene->m_constants.get(),
             .size = sizeof(CullingConstants)},
            {.binding = 1, .buffer = scene->m_instances.get(),
             .size = static_cast<std::uint64_t>(max_instances)
                * sizeof(GpuSceneInstance)},
            {.binding = 2, .buffer = scene->m_arguments.get(),
             .size = static_cast<std::uint64_t>(max_instances)
                * sizeof(RHI::DrawIndexedIndirectArguments)},
            {.binding = 3, .buffer = scene->m_visible_indices.get(),
             .size = static_cast<std::uint64_t>(max_instances)
                * sizeof(std::uint32_t)}}};
        if (result == RHI::RHIResult::Success)
            result = device.CreateResourceSet(
                {.layout = scene->m_layout.get(), .buffers = resources},
                scene->m_resource_set);
        if (result != RHI::RHIResult::Success)
            return Core::Result<std::unique_ptr<GpuDrivenScene>>::Failure(
                MakeError("RHI resource creation failed", result));
        return Core::Result<std::unique_ptr<GpuDrivenScene>>::Success(
            std::move(scene));
    }

    RHI::RHIResult GpuDrivenScene::RecordCull(
        RHI::CommandList& commands,
        std::span<const GpuSceneInstance> instances,
        const Math::Mat4& view_projection,
        std::span<const GpuSceneDraw> draws) noexcept
    {
        if (instances.empty() || instances.size() > m_max_instances
            || draws.size() != instances.size())
            return RHI::RHIResult::InvalidArgument;
        void* mapped = nullptr;
        const std::uint64_t instance_bytes =
            instances.size_bytes();
        RHI::RHIResult result = m_instances->Map(0, instance_bytes, mapped);
        if (result != RHI::RHIResult::Success) return result;
        std::memcpy(mapped, instances.data(), instances.size_bytes());
        m_instances->Unmap(0, instance_bytes);
        const CullingConstants constants = BuildConstants(
            view_projection, static_cast<std::uint32_t>(instances.size()));
        if ((result = m_constants->Map(0, sizeof(constants), mapped))
            != RHI::RHIResult::Success) return result;
        std::memcpy(mapped, &constants, sizeof(constants));
        m_constants->Unmap(0, sizeof(constants));
        const std::uint64_t argument_bytes = draws.size()
            * sizeof(RHI::DrawIndexedIndirectArguments);
        if ((result = m_argument_seed->Map(0, argument_bytes, mapped))
            != RHI::RHIResult::Success) return result;
        auto* seeds = static_cast<RHI::DrawIndexedIndirectArguments*>(mapped);
        for (std::size_t index = 0; index < draws.size(); ++index)
        {
            if (draws[index].index_count == 0)
            {
                m_argument_seed->Unmap(0, argument_bytes);
                return RHI::RHIResult::InvalidArgument;
            }
            seeds[index] = {
                .index_count = draws[index].index_count,
                .instance_count = 0,
                .first_index = draws[index].first_index,
                .vertex_offset = draws[index].vertex_offset,
                .first_instance = 0};
        }
        m_argument_seed->Unmap(0, argument_bytes);

        if ((result = commands.TransitionBuffer(*m_arguments,
                RHI::BufferState::Indirect, RHI::BufferState::CopyDestination))
                != RHI::RHIResult::Success
            || (result = commands.CopyBuffer(*m_argument_seed, 0,
                    *m_arguments, 0, argument_bytes)) != RHI::RHIResult::Success
            || (result = commands.TransitionBuffer(*m_arguments,
                    RHI::BufferState::CopyDestination, RHI::BufferState::Storage))
                != RHI::RHIResult::Success
            || (result = commands.SetComputePipeline(*m_pipeline))
                != RHI::RHIResult::Success
            || (result = commands.SetResourceSet(*m_resource_set))
                != RHI::RHIResult::Success
            || (result = commands.Dispatch(
                    (static_cast<std::uint32_t>(instances.size()) + 63u) / 64u,
                    1, 1)) != RHI::RHIResult::Success
            || (result = commands.UAVBarrier(*m_arguments))
                != RHI::RHIResult::Success
            || (result = commands.UAVBarrier(*m_visible_indices))
                != RHI::RHIResult::Success
            || (result = commands.TransitionBuffer(*m_arguments,
                    RHI::BufferState::Storage, RHI::BufferState::Indirect))
                != RHI::RHIResult::Success)
            return result;
        return RHI::RHIResult::Success;
    }

    RHI::Buffer& GpuDrivenScene::IndirectArguments() noexcept
    {
        return *m_arguments;
    }

    RHI::Buffer& GpuDrivenScene::VisibleInstanceIndices() noexcept
    {
        return *m_visible_indices;
    }

    std::uint32_t GpuDrivenScene::MaxInstances() const noexcept
    {
        return m_max_instances;
    }
}
