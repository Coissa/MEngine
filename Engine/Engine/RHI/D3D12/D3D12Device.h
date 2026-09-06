#pragma once

#include <Engine/RHI/RHIDevice.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <memory>

// Windows SDK 会把 DeviceCapabilities 定义为 A/W 宏，需在使用 RHI 同名类型前移除。
#ifdef DeviceCapabilities
#undef DeviceCapabilities
#endif

namespace GE::D3D12RHI
{
    class D3D12Queue;
    // D3D12 模块私有的逻辑设备实现；通用 Renderer 只通过 RHI::Device 使用它。
    class D3D12Device final : public RHI::Device
    {
    public:
        D3D12Device(
            RHI::AdapterId adapter_id,
            Microsoft::WRL::ComPtr<IDXGIFactory6> factory,
            Microsoft::WRL::ComPtr<ID3D12Device> native_device,
            std::unique_ptr<D3D12Queue> graphics_queue) noexcept;

        // 在源文件中定义，确保 unique_ptr 删除队列时可见完整类型。
        ~D3D12Device() noexcept override;

        [[nodiscard]]
        const RHI::DeviceCapabilities&
        GetCapabilities() const noexcept override;

        [[nodiscard]]
        RHI::Queue& GetGraphicsQueue() noexcept override;

        [[nodiscard]] RHI::RHIResult CreateFence(
            std::unique_ptr<RHI::Fence>& out_fence) override;

        [[nodiscard]] RHI::RHIResult CreateSwapchain(
            const RHI::SwapchainCreateInfo& create_info,
            std::unique_ptr<RHI::Swapchain>& out_swapchain) override;

        [[nodiscard]] RHI::RHIResult CreateColorAttachmentView(
            RHI::Texture& texture,
            std::unique_ptr<RHI::ColorAttachmentView>& out_view) override;
        [[nodiscard]] RHI::RHIResult CreateSampledTextureView(
            RHI::Texture& texture,
            std::unique_ptr<RHI::SampledTextureView>& out_view) override;
        [[nodiscard]] RHI::RHIResult CreateTexture(
            const RHI::TextureDescription& description,
            RHI::TextureState initial_state,
            std::unique_ptr<RHI::Texture>& out_texture) override;
        [[nodiscard]] RHI::RHIResult GetTextureUploadLayout(
            const RHI::TextureDescription& description,
            RHI::TextureSubresource subresource,
            std::uint64_t base_offset,
            RHI::TextureUploadLayout& out_layout) const noexcept override;
        [[nodiscard]] RHI::RHIResult CreateDepthStencilView(
            RHI::Texture& texture,
            std::unique_ptr<RHI::DepthStencilView>& out_view) override;
        [[nodiscard]] RHI::RHIResult CreateBuffer(
            const RHI::BufferDescription& description,
            RHI::BufferState initial_state,
            std::unique_ptr<RHI::Buffer>& out_buffer) override;
        [[nodiscard]] RHI::RHIResult CreateSampler(
            const RHI::SamplerDescription& description,
            std::unique_ptr<RHI::Sampler>& out_sampler) override;
        [[nodiscard]] RHI::RHIResult CreateShader(
            const RHI::ShaderDescription& description,
            std::unique_ptr<RHI::Shader>& out_shader) override;
        [[nodiscard]] RHI::RHIResult CreatePipelineLayout(
            const RHI::PipelineLayoutDescription& description,
            std::unique_ptr<RHI::PipelineLayout>& out_layout) override;
        [[nodiscard]] RHI::RHIResult CreateResourceSet(
            const RHI::ResourceSetDescription& description,
            std::unique_ptr<RHI::ResourceSet>& out_resource_set) override;
        [[nodiscard]] RHI::RHIResult CreateGraphicsPipeline(
            const RHI::GraphicsPipelineDescription& description,
            std::unique_ptr<RHI::GraphicsPipeline>& out_pipeline) override;
        [[nodiscard]] RHI::RHIResult CreateCommandAllocator(
            std::unique_ptr<RHI::CommandAllocator>& out_allocator) override;
        [[nodiscard]] RHI::RHIResult CreateCommandList(
            RHI::CommandAllocator& allocator,
            std::unique_ptr<RHI::CommandList>& out_command_list) override;
    private:
        // 对外报告已创建设备真正具备的通用能力。
        RHI::DeviceCapabilities capabilities_;

        // Device 保留 DXGI Factory，确保 Backend 销毁后仍可创建交换链。
        Microsoft::WRL::ComPtr<IDXGIFactory6> factory_;

        // 持有 ID3D12Device 的 COM 引用，析构时由 ComPtr 自动 Release。
        Microsoft::WRL::ComPtr<ID3D12Device> native_device_;

        // 成员逆序析构：先释放队列，再释放原生设备。
        std::unique_ptr<D3D12Queue> graphics_queue_;

    };
}
