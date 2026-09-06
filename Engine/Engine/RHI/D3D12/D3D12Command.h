#pragma once

#include <Engine/RHI/RHICommand.h>

#include <d3d12.h>
#include <wrl/client.h>

namespace GE::D3D12RHI
{
    class D3D12Device;
    class D3D12Queue;

    class D3D12CommandAllocator final : public RHI::CommandAllocator
    {
    public:
        explicit D3D12CommandAllocator(
            Microsoft::WRL::ComPtr<ID3D12CommandAllocator> native_allocator) noexcept;
        ~D3D12CommandAllocator() noexcept override = default;
        [[nodiscard]] RHI::RHIResult Reset() noexcept override;

    private:
        friend class D3D12Device;
        friend class D3D12CommandList;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> native_allocator_;
    };

    class D3D12CommandList final : public RHI::CommandList
    {
    public:
        explicit D3D12CommandList(
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> native_list) noexcept;
        ~D3D12CommandList() noexcept override = default;

        [[nodiscard]] RHI::RHIResult Reset(
            RHI::CommandAllocator& allocator) noexcept override;
        [[nodiscard]] RHI::RHIResult TransitionTexture(
            RHI::Texture& texture,
            RHI::TextureState before,
            RHI::TextureState after) noexcept override;
        [[nodiscard]] RHI::RHIResult TransitionBuffer(
            RHI::Buffer& buffer,
            RHI::BufferState before,
            RHI::BufferState after) noexcept override;
        [[nodiscard]] RHI::RHIResult CopyBuffer(
            RHI::Buffer& source,
            std::uint64_t source_offset,
            RHI::Buffer& destination,
            std::uint64_t destination_offset,
            std::uint64_t size) noexcept override;
        [[nodiscard]] RHI::RHIResult CopyBufferToTexture(
            RHI::Buffer& source,
            RHI::Texture& destination,
            const RHI::TextureUploadLayout& layout) noexcept override;
        [[nodiscard]] RHI::RHIResult SetColorAttachment(
            RHI::ColorAttachmentView& view) noexcept override;
        [[nodiscard]] RHI::RHIResult SetColorAttachmentOnly(
            RHI::ColorAttachmentView& view) noexcept override;
        [[nodiscard]] RHI::RHIResult SetDepthStencilAttachment(
            RHI::DepthStencilView& view) noexcept override;
        [[nodiscard]] RHI::RHIResult ClearColorAttachment(
            RHI::ColorAttachmentView& view,
            const RHI::ClearColor& color) noexcept override;
        [[nodiscard]] RHI::RHIResult ClearDepthStencilAttachment(
            RHI::DepthStencilView& view,
            float depth) noexcept override;
        [[nodiscard]] RHI::RHIResult SetViewportAndScissor(
            const RHI::Viewport& viewport,
            const RHI::ScissorRect& scissor) noexcept override;
        [[nodiscard]] RHI::RHIResult SetGraphicsPipeline(
            RHI::GraphicsPipeline& pipeline) noexcept override;
        [[nodiscard]] RHI::RHIResult SetResourceSet(
            RHI::ResourceSet& resource_set) noexcept override;
        [[nodiscard]] RHI::RHIResult SetVertexBuffer(
            RHI::Buffer& buffer,
            std::uint64_t offset,
            std::uint32_t stride) noexcept override;
        [[nodiscard]] RHI::RHIResult SetIndexBuffer(
            RHI::Buffer& buffer,
            std::uint64_t offset,
            RHI::RHIFormat index_format) noexcept override;
        [[nodiscard]] RHI::RHIResult DrawIndexed(
            std::uint32_t index_count,
            std::uint32_t instance_count,
            std::uint32_t first_index,
            std::int32_t vertex_offset,
            std::uint32_t first_instance) noexcept override;
        [[nodiscard]] RHI::RHIResult Close() noexcept override;

    private:
        friend class D3D12Queue;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> native_list_;
        bool recording_ = true;
        bool has_color_attachment_ = false;
        bool has_depth_attachment_ = false;
        D3D12_CPU_DESCRIPTOR_HANDLE color_attachment_{};
        D3D12_CPU_DESCRIPTOR_HANDLE depth_attachment_{};
        const void* current_layout_ = nullptr;
    };
}
