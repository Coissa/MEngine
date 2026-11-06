#include "MotionVectorPass.h"

namespace GE::Render
{
    void MotionVectorPass::Execute(RHI::CommandList& cmd, const MotionVectorPassInput& input) const
    {
        cmd.Transition(input.output.Resource(), RHI::State::RenderTarget);
        cmd.SetRenderTargets(input.output);
        cmd.Transition(input.velocity.Resource(), RHI::State::ShaderResource);
        const auto& size = input.output.Resource().Description();
        cmd.SetViewport(size.width, size.height);
        cmd.SetPipeline(input.pipeline);
        cmd.SetBindingLayout(input.layout);
        cmd.BindConstantBuffer(0, input.constants);
        cmd.BindTexture(1, input.velocity);
        cmd.Draw(3);
    }
} // namespace GE::Render
