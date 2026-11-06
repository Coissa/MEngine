#pragma once
#include "../RHI/RHICommandList.h"

namespace GE::Render
{
    struct MotionVectorPassInput
    {
        RHI::TextureView& output;
        RHI::TextureView& velocity;
        RHI::GraphicsPipeline& pipeline;
        RHI::BindingLayout& layout;
        RHI::Buffer& constants;
    };

    class MotionVectorPass
    {
    public:
        void Execute(RHI::CommandList&, const MotionVectorPassInput&) const;
    };
} // namespace GE::Render
