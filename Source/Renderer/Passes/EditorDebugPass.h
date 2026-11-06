#pragma once
#include "../RHI/RHICommandList.h"
namespace GE::Render
{
    struct EditorDebugPassInput
    {
        RHI::TextureView& color;
        RHI::TextureView& depth;
        RHI::GraphicsPipeline& visible;
        RHI::GraphicsPipeline& occluded;
        RHI::BindingLayout& layout;
        RHI::Buffer& vertices;
        RHI::Buffer& indices;
        uint32_t indexCount;
    };
    class EditorDebugPass
    {
    public:
        void Execute(RHI::CommandList&, const EditorDebugPassInput&) const;
    };
} // namespace GE::Render
