#pragma once

#include "RHIDeviceCapabilities.h"
#include "RHIFence.h"
#include "RHIQueue.h"
#include "RHISwapchain.h"
#include "RHICommand.h"
#include "RHIBuffer.h"
#include "RHIPipeline.h"
#include "RHIShader.h"
#include "RHITextureView.h"

#include <memory>

namespace GE::RHI
{
    class Device
    {
    public:
        virtual ~Device() noexcept = default;

        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;
        Device(Device&&) = delete;
        Device& operator=(Device&&) = delete;

        [[nodiscard]]
        virtual const DeviceCapabilities&
        GetCapabilities() const noexcept = 0;

        // 成功初始化的设备始终拥有主图形队列；借用引用不得超出设备生命周期。
        // 重复获取返回同一对象，不进行分配或创建新的原生队列。
        [[nodiscard]]
        virtual Queue& GetGraphicsQueue() noexcept = 0;

        // 创建初始完成值为 0 的 Fence；失败时输出保持为空。
        [[nodiscard]] virtual RHIResult CreateFence(
            std::unique_ptr<Fence>& out_fence) = 0;

        // 使用设备的主图形队列创建窗口交换链；失败时输出保持为空。
        [[nodiscard]] virtual RHIResult CreateSwapchain(
            const SwapchainCreateInfo& create_info,
            std::unique_ptr<Swapchain>& out_swapchain) = 0;

        [[nodiscard]] virtual RHIResult CreateColorAttachmentView(
            Texture& texture,
            std::unique_ptr<ColorAttachmentView>& out_view) = 0;

        [[nodiscard]] virtual RHIResult CreateSampledTextureView(
            Texture& texture,
            const SampledTextureViewDescription& description,
            std::unique_ptr<SampledTextureView>& out_view) = 0;

        [[nodiscard]] virtual RHIResult CreateTexture(
            const TextureDescription& description,
            TextureState initial_state,
            std::unique_ptr<Texture>& out_texture) = 0;

        // 查询当前后端认可的线性上传布局；调用方按 row_pitch 逐行写入 Buffer。
        [[nodiscard]] virtual RHIResult GetTextureUploadLayout(
            const TextureDescription& description,
            TextureSubresource subresource,
            std::uint64_t base_offset,
            TextureUploadLayout& out_layout) const noexcept = 0;

        [[nodiscard]] virtual RHIResult CreateDepthStencilView(
            Texture& texture,
            std::unique_ptr<DepthStencilView>& out_view) = 0;

        [[nodiscard]] virtual RHIResult CreateBuffer(
            const BufferDescription& description,
            BufferState initial_state,
            std::unique_ptr<Buffer>& out_buffer) = 0;

        [[nodiscard]] virtual RHIResult CreateSampler(
            const SamplerDescription& description,
            std::unique_ptr<Sampler>& out_sampler) = 0;

        [[nodiscard]] virtual RHIResult CreateShader(
            const ShaderDescription& description,
            std::unique_ptr<Shader>& out_shader) = 0;

        [[nodiscard]] virtual RHIResult CreatePipelineLayout(
            const PipelineLayoutDescription& description,
            std::unique_ptr<PipelineLayout>& out_layout) = 0;

        [[nodiscard]] virtual RHIResult CreateResourceSet(
            const ResourceSetDescription& description,
            std::unique_ptr<ResourceSet>& out_resource_set) = 0;

        [[nodiscard]] virtual RHIResult CreateGraphicsPipeline(
            const GraphicsPipelineDescription& description,
            std::unique_ptr<GraphicsPipeline>& out_pipeline) = 0;

        [[nodiscard]] virtual RHIResult CreateCommandAllocator(
            std::unique_ptr<CommandAllocator>& out_allocator) = 0;

        [[nodiscard]] virtual RHIResult CreateCommandList(
            CommandAllocator& allocator,
            std::unique_ptr<CommandList>& out_command_list) = 0;

    protected:
        Device() noexcept = default;
    };
}
