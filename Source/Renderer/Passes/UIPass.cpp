#include "UIPass.h"
#include <algorithm>
namespace GE::Render
{
    void UIPass::Execute(RHI::CommandList& cmd, const UIPassInput& input) const
    {
        cmd.Transition(input.scene.Resource(), RHI::State::ShaderResource);
        cmd.Transition(input.output.Resource(), RHI::State::RenderTarget);
        cmd.SetRenderTargets(input.output);
        const auto& size = input.output.Resource().Description();
        cmd.SetViewport(size.width, size.height);
        cmd.ClearColor(input.output, {0.035f, 0.038f, 0.048f, 1});
        if (input.data.IsEmpty())
            return;
        cmd.SetPipeline(input.pipeline);
        cmd.SetBindingLayout(input.layout);
        cmd.BindConstantBuffer(0, input.constants);
        cmd.BindSampler(2, input.sampler);
        cmd.SetVertexBuffer(input.vertices, sizeof(RenderCore::UIVertex));
        cmd.SetIndexBuffer(input.indices, RHI::RHIFormat::R32_UInt);
        for (const auto& draw : input.data.commands)
        {
            const auto& data = input.data;
            int l = (std::max)(0, int((draw.clip_rectangle[0] - data.display_position[0]) * data.framebuffer_scale[0]));
            int t = (std::max)(0, int((draw.clip_rectangle[1] - data.display_position[1]) * data.framebuffer_scale[1]));
            int r = (std::min)(int(size.width),
                               int((draw.clip_rectangle[2] - data.display_position[0]) * data.framebuffer_scale[0]));
            int b = (std::min)(int(size.height),
                               int((draw.clip_rectangle[3] - data.display_position[1]) * data.framebuffer_scale[1]));
            if (l >= r || t >= b)
                continue;
            cmd.SetScissor(l, t, r, b);
            cmd.BindTexture(1, draw.texture == RenderCore::UITexture::SceneViewport ? input.scene : input.font);
            cmd.DrawIndexed(draw.index_count, 1, draw.first_index, draw.vertex_offset);
        }
    }
} // namespace GE::Render
