#pragma once
#include "../PipelineLibrary.h"
#include "../RenderResources.h"
namespace GE::Render
{
    struct SkyInput
    {
        RHI::GraphicsPipeline& pipeline;
        RHI::BindingLayout& layout;
        RHI::Buffer& constants;
        RHI::Buffer& vertices;
        RHI::Buffer& indices;
        RHI::TextureView& texture;
        RHI::Sampler& sampler;
    };
    struct ScenePassInput
    {
        RHI::TextureView& color;
        RHI::TextureView& depth;
        RHI::TextureView& velocity;
        RHI::GraphicsPipeline& pipeline;
        RHI::BindingLayout& layout;
        RHI::Buffer& frame;
        RHI::Buffer& objects;
        std::span<const DrawPacket> draws;
        RHI::Buffer* indirect = nullptr;
        const SceneAssets* assets = nullptr;
        RHI::Buffer* lighting = nullptr;
    };
    class ScenePass
    {
    public:
        void Execute(RHI::CommandList&, const ScenePassInput&) const;
        void DrawShadows(RHI::CommandList&, const SceneAssets&, const PipelineLibrary&, RHI::Buffer& cascadeConstants,
                         RHI::Buffer& objects, std::span<const DrawPacket>) const;
        void DrawSky(RHI::CommandList&, RHI::TextureView& color, RHI::TextureView& depth, RHI::TextureView& velocity,
                     const SkyInput&) const;
    };
} // namespace GE::Render
