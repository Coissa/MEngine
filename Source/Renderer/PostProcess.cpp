#include "PostProcess.h"

namespace GE::Render
{
    namespace
    {
        float Halton(uint32_t index, uint32_t base) noexcept
        {
            float value = 0, fraction = 1;
            while (index != 0)
            {
                fraction /= float(base);
                value += fraction * float(index % base);
                index /= base;
            }
            return value;
        }
    } // namespace

    std::array<float, 2> TemporalJitter(uint32_t index) noexcept
    {
        const auto sample = index % 8 + 1;
        return {Halton(sample, 2) - 0.5f, Halton(sample, 3) - 0.5f};
    }

    void ResolveTAA(RHI::CommandList& cmd, const TAAInput& input)
    {
        cmd.Transition(input.output.Resource(), RHI::State::RenderTarget);
        cmd.SetRenderTargets(input.output);
        cmd.Transition(input.color.Resource(), RHI::State::ShaderResource);
        cmd.Transition(input.velocity.Resource(), RHI::State::ShaderResource);
        cmd.Transition(input.depth.Resource(), RHI::State::ShaderResource);
        cmd.Transition(input.history.Resource(), RHI::State::ShaderResource);
        const auto& size = input.output.Resource().Description();
        cmd.SetViewport(size.width, size.height);
        cmd.SetPipeline(input.pipeline);
        cmd.SetBindingLayout(input.layout);
        cmd.BindConstantBuffer(0, input.constants);
        cmd.BindTexture(1, input.color);
        cmd.BindTexture(2, input.velocity);
        cmd.BindTexture(3, input.depth);
        cmd.BindTexture(4, input.history);
        cmd.Draw(3);
    }

    void DrawSceneOutput(RHI::CommandList& cmd, const SceneOutputInput& input)
    {
        cmd.Transition(input.output.Resource(), RHI::State::RenderTarget);
        cmd.SetRenderTargets(input.output);
        cmd.Transition(input.history.Resource(), RHI::State::ShaderResource);
        const auto& size = input.output.Resource().Description();
        cmd.SetViewport(size.width, size.height);
        cmd.SetPipeline(input.pipeline);
        cmd.SetBindingLayout(input.layout);
        cmd.BindTexture(0, input.history);
        cmd.Draw(3);
    }

    void ToneMapScene(RHI::CommandList& cmd, const SceneOutputInput& input, RHI::Buffer& constants)
    {
        cmd.Transition(input.output.Resource(), RHI::State::RenderTarget);
        cmd.SetRenderTargets(input.output);
        cmd.Transition(input.history.Resource(), RHI::State::ShaderResource);
        const auto& size = input.output.Resource().Description();
        cmd.SetViewport(size.width, size.height);
        cmd.SetPipeline(input.pipeline);
        cmd.SetBindingLayout(input.layout);
        cmd.BindConstantBuffer(0, constants);
        cmd.BindTexture(1, input.history);
        cmd.Draw(3);
    }
} // namespace GE::Render
