#pragma once
#include "../RHIDevice.h"
#include "D3D12CommandContext.h"
#include "D3D12DescriptorAllocator.h"
#include <dxgi1_6.h>

namespace GE::D3D12
{
    class Device final : public RHI::Device
    {
    public:
        explicit Device(const RHI::DeviceDesc& desc);
        ~Device() override;

        const RHI::DeviceCaps& GetCapabilities() const noexcept override { return capabilities; }

        RHI::GraphicsQueue& GetGraphicsQueue() override { return *queue; }

        RHI::CommandContext& GetCommandContext() override { return *context; }

        std::unique_ptr<RHI::Buffer> CreateBuffer(const RHI::BufferDesc& desc) override;
        std::unique_ptr<RHI::Texture> CreateTexture(const RHI::TextureDesc& desc) override;
        std::unique_ptr<RHI::TimestampQuery> CreateTimestampQuery(uint32_t count) override;
        std::unique_ptr<RHI::TextureView> CreateView(RHI::Texture& texture, RHI::ViewKind kind,
                                                     const RHI::SampledTextureViewDescription& view = {}) override;
        std::unique_ptr<RHI::BindingLayout> CreateBindingLayout(const RHI::BindingLayoutDesc& desc) override;
        std::unique_ptr<RHI::GraphicsPipeline> CreateGraphicsPipeline(const RHI::GraphicsPipelineDesc& desc) override;
        void UploadBuffer(RHI::Buffer& buffer, const void* data, size_t bytes) override;
        void UploadTexture(RHI::Texture& texture, const void* rgba, uint32_t sourceRowPitch, uint32_t mip,
                           uint32_t layer) override;
        RHI::Image ReadbackTexture(RHI::Texture& texture) override;
        std::unique_ptr<RHI::Sampler> CreateSampler(const RHI::SamplerDescription&) override;
        std::unique_ptr<RHI::ComputePipeline> CreateComputePipeline(const RHI::ComputePipelineDesc&) override;
        void CheckDebugErrors() override;
        std::unique_ptr<RHI::RayTracingMesh> CreateRayTracingMesh(RHI::Buffer&, uint32_t, uint32_t, RHI::Buffer&,
                                                                  uint32_t) override;
        std::unique_ptr<RHI::RayTracingScene> CreateRayTracingScene(uint32_t capacity) override;

        ID3D12Device* Native() const { return native.Get(); }

        IDXGIFactory4* Factory() const { return factory.Get(); }

        GraphicsQueue& Queue() const { return *queue; }

    private:
        // Reverse destruction: context -> descriptors -> queue -> native device ->
        // factory.
        RHI::DeviceCaps capabilities;
        ComPtr<IDXGIFactory6> factory;
        ComPtr<ID3D12Device> native;
        std::unique_ptr<GraphicsQueue> queue;
        std::unique_ptr<DescriptorAllocator> rtvDescriptors;
        std::unique_ptr<DescriptorAllocator> dsvDescriptors;
        std::unique_ptr<DescriptorAllocator> shaderDescriptors;
        std::unique_ptr<DescriptorAllocator> samplerDescriptors;
        std::unique_ptr<CommandContext> context;
    };
} // namespace GE::D3D12
