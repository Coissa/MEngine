#include "EditorDebugPass.h"
#include "../RenderData.h"
namespace GE::Render
{
    void EditorDebugPass::Execute(RHI::CommandList& cmd, const EditorDebugPassInput& input) const
    {
        cmd.Transition(input.color.Resource(), RHI::State::RenderTarget);
        cmd.Transition(input.depth.Resource(), RHI::State::DepthWrite);
        cmd.SetRenderTargets(input.color, &input.depth);
        const auto& size = input.color.Resource().Description();
        cmd.SetViewport(size.width, size.height);
        cmd.SetPipeline(input.occluded);
        cmd.SetBindingLayout(input.layout);
        cmd.SetVertexBuffer(input.vertices, sizeof(RenderCore::GizmoVertex));
        cmd.SetIndexBuffer(input.indices, RHI::RHIFormat::R32_UInt);
        cmd.DrawIndexed(input.indexCount);
        cmd.SetPipeline(input.visible);
        cmd.DrawIndexed(input.indexCount);
    }
} // namespace GE::Render
