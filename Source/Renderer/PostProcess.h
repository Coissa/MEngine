#pragma once
#include "RHI/RHICommandList.h"

namespace GE::Render
{
    // Pixel offsets, positive towards the right and bottom of the viewport.
    std::array<float, 2> TemporalJitter(uint32_t index) noexcept;

    struct TAAConstants
    {
        std::array<float, 2> inverseSize, jitterUV;
        float currentWeight = 0.1f;
        uint32_t historyValid = 0;
        std::array<float, 2> padding{};
    };

    struct TAAInput
    {
        RHI::TextureView& color;
        RHI::TextureView& velocity;
        RHI::TextureView& depth;
        RHI::TextureView& history;
        RHI::TextureView& output;
        RHI::GraphicsPipeline& pipeline;
        RHI::BindingLayout& layout;
        RHI::Buffer& constants;
    };

    void ResolveTAA(RHI::CommandList&, const TAAInput&);

    struct SceneOutputInput
    {
        RHI::TextureView& history;
        RHI::TextureView& output;
        RHI::GraphicsPipeline& pipeline;
        RHI::BindingLayout& layout;
    };

    void DrawSceneOutput(RHI::CommandList&, const SceneOutputInput&);
    void ToneMapScene(RHI::CommandList&, const SceneOutputInput&, RHI::Buffer& constants);
} // namespace GE::Render
