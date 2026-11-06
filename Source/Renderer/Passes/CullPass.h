#pragma once
#include "../RHI/RHIDevice.h"
#include <Core/Math/matrix4.h>
namespace GE::Render
{
    struct CullInstance
    {
        std::array<float, 3> center;
        float radius;
    };
    struct CullResources
    {
        std::unique_ptr<RHI::Buffer> instances, constants, seed, arguments, visible;
        CullResources(RHI::Device&, uint32_t capacity);
    };
    struct CullPassInput
    {
        RHI::ComputePipeline& pipeline;
        RHI::BindingLayout& layout;
        CullResources& resources;
        std::span<const CullInstance> instances;
        std::span<const RHI::DrawIndexedIndirectArguments> seeds;
        const Math::Mat4& viewProjection;
    };
    class CullPass
    {
    public:
        void Execute(RHI::CommandList&, const CullPassInput&) const;
    };
} // namespace GE::Render
