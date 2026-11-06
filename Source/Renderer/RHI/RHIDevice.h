#pragma once
#include "RHICommandContext.h"
#include "RHIGraphicsQueue.h"
#include <memory>

namespace GE::RHI
{
    class Device
    {
    public:
        virtual ~Device() = default;
        virtual const DeviceCaps& GetCapabilities() const noexcept = 0;
        virtual GraphicsQueue& GetGraphicsQueue() = 0;
        virtual CommandContext& GetCommandContext() = 0;
        virtual std::unique_ptr<Buffer> CreateBuffer(const BufferDesc& desc) = 0;
        virtual std::unique_ptr<Texture> CreateTexture(const TextureDesc& desc) = 0;
        virtual std::unique_ptr<TimestampQuery> CreateTimestampQuery(uint32_t count) = 0;
        virtual std::unique_ptr<TextureView> CreateView(Texture& texture, ViewKind kind,
                                                        const SampledTextureViewDescription& view = {}) = 0;
        virtual std::unique_ptr<BindingLayout> CreateBindingLayout(const BindingLayoutDesc& desc) = 0;
        virtual std::unique_ptr<GraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
        virtual std::unique_ptr<Sampler> CreateSampler(const SamplerDescription& desc) = 0;
        virtual std::unique_ptr<ComputePipeline> CreateComputePipeline(const ComputePipelineDesc& desc) = 0;
        // Synchronous helpers. No normal frame recording may be active.
        virtual void UploadBuffer(Buffer& buffer, const void* data, size_t bytes) = 0;
        virtual void UploadTexture(Texture& texture, const void* rgba, uint32_t sourceRowPitch, uint32_t mip = 0,
                                   uint32_t layer = 0) = 0;
        virtual Image ReadbackTexture(Texture& texture) = 0; // RGBA8 output in this example.
        virtual void CheckDebugErrors() = 0;                 // Used by the smoke run, not normal Pass code.
    };
} // namespace GE::RHI
