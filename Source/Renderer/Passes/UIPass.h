#pragma once
#include "../RHI/RHICommandList.h"
#include "../RenderData.h"
namespace GE::Render
{
    struct UIPassInput
    {
        RHI::TextureView& output;
        RHI::TextureView& scene;
        RHI::TextureView& font;
        RHI::Sampler& sampler;
        RHI::GraphicsPipeline& pipeline;
        RHI::BindingLayout& layout;
        RHI::Buffer& constants;
        RHI::Buffer& vertices;
        RHI::Buffer& indices;
        const RenderCore::UIRenderData& data;
    };
    class UIPass
    {
    public:
        void Execute(RHI::CommandList&, const UIPassInput&) const;
    };
} // namespace GE::Render
