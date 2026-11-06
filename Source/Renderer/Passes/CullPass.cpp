#include "CullPass.h"
#include <cmath>
namespace GE::Render
{
    namespace
    {
        struct alignas(16) CullingConstants final
        {
            std::array<std::array<float, 4>, 6> planes{};
            std::uint32_t instance_count = 0;
            std::array<std::uint32_t, 3> padding{};
        };

        [[nodiscard]] std::array<float, 4> NormalizePlane(std::array<float, 4> plane) noexcept
        {
            const float length = std::sqrt(plane[0] * plane[0] + plane[1] * plane[1] + plane[2] * plane[2]);
            if (length > 0.0F)
                for (float& value : plane)
                    value /= length;
            return plane;
        }

        [[nodiscard]] CullingConstants BuildConstants(const Math::Mat4& matrix, std::uint32_t count) noexcept
        {
            const auto column = [&matrix](std::size_t index)
            {
                return std::array<float, 4>{matrix.At(0, index), matrix.At(1, index), matrix.At(2, index),
                                            matrix.At(3, index)};
            };
            const auto add = [](const auto& left, const auto& right, float sign)
            {
                return NormalizePlane({left[0] + sign * right[0], left[1] + sign * right[1], left[2] + sign * right[2],
                                       left[3] + sign * right[3]});
            };
            const auto x = column(0);
            const auto y = column(1);
            const auto z = column(2);
            const auto w = column(3);
            CullingConstants result;
            result.planes = {add(w, x, 1.0F),  add(w, x, -1.0F),  add(w, y, 1.0F),
                             add(w, y, -1.0F), NormalizePlane(z), add(w, z, -1.0F)};
            result.instance_count = count;
            return result;
        }
    } // namespace

    CullResources::CullResources(RHI::Device& device, uint32_t capacity)
    {
        instances = device.CreateBuffer({capacity * sizeof(CullInstance), RHI::Memory::Upload});
        constants = device.CreateBuffer({256, RHI::Memory::Upload});
        seed = device.CreateBuffer({capacity * sizeof(RHI::DrawIndexedIndirectArguments), RHI::Memory::Upload});
        arguments = device.CreateBuffer({capacity * sizeof(RHI::DrawIndexedIndirectArguments), RHI::Memory::GPU, true});
        visible = device.CreateBuffer({capacity * sizeof(uint32_t), RHI::Memory::GPU, true});
    }
    void CullPass::Execute(RHI::CommandList& cmd, const CullPassInput& input) const
    {
        auto& resources = input.resources;
        resources.instances->Write(0, input.instances.data(), input.instances.size_bytes());
        const auto constants = BuildConstants(input.viewProjection, uint32_t(input.instances.size()));
        resources.constants->Write(0, &constants, sizeof(constants));
        resources.seed->Write(0, input.seeds.data(), input.seeds.size_bytes());
        cmd.Transition(*resources.arguments, RHI::State::CopyDestination);
        cmd.CopyBuffer(*resources.seed, *resources.arguments, input.seeds.size_bytes());
        cmd.Transition(*resources.arguments, RHI::State::Storage);
        cmd.Transition(*resources.visible, RHI::State::Storage);
        cmd.SetComputePipeline(input.pipeline);
        cmd.SetBindingLayout(input.layout);
        cmd.BindConstantBuffer(0, *resources.constants);
        cmd.BindBuffer(1, *resources.instances);
        cmd.BindBuffer(2, *resources.arguments, true);
        cmd.BindBuffer(3, *resources.visible, true);
        cmd.Dispatch((uint32_t(input.instances.size()) + 63) / 64);
        cmd.UAVBarrier(*resources.arguments);
        cmd.UAVBarrier(*resources.visible);
        cmd.Transition(*resources.arguments, RHI::State::Indirect);
    }
} // namespace GE::Render
