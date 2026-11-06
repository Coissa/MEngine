#include "ScenePass.h"
namespace GE::Render
{
    void ScenePass::Execute(RHI::CommandList& cmd, const ScenePassInput& input) const
    {
        cmd.Transition(input.color.Resource(), RHI::State::RenderTarget);
        cmd.Transition(input.depth.Resource(), RHI::State::DepthWrite);
        cmd.Transition(input.velocity.Resource(), RHI::State::RenderTarget);
        std::array<RHI::TextureView*, 4> targets{&input.color, &input.velocity, input.giNormal, input.giDiffuse};
        if (input.giNormal)
        {
            cmd.Transition(input.giNormal->Resource(), RHI::State::RenderTarget);
            cmd.Transition(input.giDiffuse->Resource(), RHI::State::RenderTarget);
            cmd.ClearColor(*input.giNormal, {0, 0, 0, 0});
            cmd.ClearColor(*input.giDiffuse, {0, 0, 0, 0});
        }
        cmd.SetRenderTargets(std::span(targets.data(), input.giNormal ? 4u : 2u), &input.depth);
        const auto& size = input.color.Resource().Description();
        cmd.SetViewport(size.width, size.height);
        cmd.ClearColor(input.color, {0.025f, 0.04f, 0.085f, 1});
        cmd.ClearDepth(input.depth);
        cmd.ClearColor(input.velocity, {0, 0, 0, 0});
        cmd.SetPipeline(input.pipeline);
        cmd.SetBindingLayout(input.layout);
        cmd.BindConstantBuffer(0, input.frame);
        cmd.BindConstantBuffer(8, *input.lighting);
        const auto& assets = *input.assets;
        const std::array<RHI::TextureView*, 4> lightingTextures{
            assets.irradiance.view.get(), assets.prefilter.view.get(), assets.brdf.view.get(), assets.shadowSRV.get()};
        for (auto* view : lightingTextures)
            cmd.Transition(view->Resource(), RHI::State::ShaderResource);
        cmd.BindTexture(9, *lightingTextures[0]);
        cmd.BindTexture(10, *lightingTextures[1]);
        cmd.BindTexture(11, *lightingTextures[2]);
        cmd.BindSampler(12, *assets.environmentSampler);
        cmd.BindTexture(13, *lightingTextures[3]);
        for (size_t i = 0; i < input.draws.size(); ++i)
        {
            const auto& draw = input.draws[i];
            cmd.BindConstantBuffer(1, input.objects, draw.objectOffset);
            cmd.Transition(draw.texture->Resource(), RHI::State::ShaderResource);
            cmd.BindTexture(2, *draw.texture);
            cmd.BindSampler(3, *draw.sampler);
            for (uint32_t texture = 0; texture < draw.materialTextures.size(); ++texture)
            {
                cmd.Transition(draw.materialTextures[texture]->Resource(), RHI::State::ShaderResource);
                cmd.BindTexture(4 + texture, *draw.materialTextures[texture]);
            }
            cmd.Transition(*draw.vertices, RHI::State::VertexBuffer);
            cmd.Transition(*draw.indices, RHI::State::IndexBuffer);
            cmd.SetVertexBuffer(*draw.vertices, sizeof(PositionNormalColorTextureVertex));
            cmd.SetIndexBuffer(*draw.indices);
            if (draw.nanite)
                for (const auto& range : draw.ranges)
                    cmd.DrawIndexed(range.count, 1, range.firstIndex, range.vertexOffset);
            else if (input.indirect)
                cmd.ExecuteIndirect(*input.indirect, i * sizeof(RHI::DrawIndexedIndirectArguments));
            else
                cmd.DrawIndexed(draw.indexCount);
        }
    }

    void ScenePass::DrawShadows(RHI::CommandList& cmd, const SceneAssets& assets, const PipelineLibrary& pipelines,
                                RHI::Buffer& cascades, RHI::Buffer& objects, std::span<const DrawPacket> draws) const
    {
        cmd.SetPipeline(*pipelines.shadow);
        cmd.SetBindingLayout(*pipelines.shadowLayout);
        cmd.SetViewport(ShadowResolution, ShadowResolution);
        for (uint32_t cascade = 0; cascade < ShadowCascades; ++cascade)
        {
            cmd.Transition(*assets.shadowMap, RHI::State::DepthWrite, cascade);
            cmd.SetRenderTargets(std::span<RHI::TextureView* const>{}, assets.shadowDSV[cascade].get());
            cmd.ClearDepth(*assets.shadowDSV[cascade]);
            cmd.BindConstantBuffer(0, cascades, cascade * 256);
            for (const auto& draw : draws)
            {
                // Camera culling is deliberately not reused: off-screen objects can cast visible shadows.
                if (draw.nanite)
                    continue; // Cluster visualization is not scene lighting.
                cmd.BindConstantBuffer(1, objects, draw.objectOffset);
                cmd.Transition(draw.texture->Resource(), RHI::State::ShaderResource);
                cmd.BindTexture(2, *draw.texture);
                cmd.BindSampler(3, *draw.sampler);
                cmd.Transition(*draw.vertices, RHI::State::VertexBuffer);
                cmd.Transition(*draw.indices, RHI::State::IndexBuffer);
                cmd.SetVertexBuffer(*draw.vertices, sizeof(PositionNormalColorTextureVertex));
                cmd.SetIndexBuffer(*draw.indices);
                cmd.DrawIndexed(draw.indexCount);
            }
            cmd.Transition(*assets.shadowMap, RHI::State::ShaderResource, cascade);
        }
    }

    void ScenePass::DrawSky(RHI::CommandList& cmd, RHI::TextureView& color, RHI::TextureView& depth,
                            RHI::TextureView& velocity, const SkyInput& sky) const
    {
        cmd.Transition(color.Resource(), RHI::State::RenderTarget);
        cmd.Transition(depth.Resource(), RHI::State::DepthWrite);
        cmd.Transition(velocity.Resource(), RHI::State::RenderTarget);
        std::array<RHI::TextureView*, 2> targets{&color, &velocity};
        cmd.SetRenderTargets(targets, &depth);
        const auto& size = color.Resource().Description();
        cmd.SetViewport(size.width, size.height);
        cmd.SetPipeline(sky.pipeline);
        cmd.SetBindingLayout(sky.layout);
        cmd.BindConstantBuffer(0, sky.constants);
        cmd.Transition(sky.texture.Resource(), RHI::State::ShaderResource);
        cmd.BindTexture(1, sky.texture);
        cmd.BindSampler(2, sky.sampler);
        cmd.Transition(sky.vertices, RHI::State::VertexBuffer);
        cmd.Transition(sky.indices, RHI::State::IndexBuffer);
        cmd.SetVertexBuffer(sky.vertices, 12);
        cmd.SetIndexBuffer(sky.indices);
        cmd.DrawIndexed(36);
    }
} // namespace GE::Render
