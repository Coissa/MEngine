#include "D3D12TextureView.h"
#include "D3D12Texture.h"

#include <utility>

namespace GE::D3D12RHI
{
    D3D12SampledTextureView::D3D12SampledTextureView(
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap,
        D3D12_CPU_DESCRIPTOR_HANDLE descriptor,
        D3D12Texture& texture) noexcept
        : descriptor_heap_(std::move(descriptor_heap)),
          descriptor_(descriptor),
          texture_(&texture)
    {
    }

    RHI::Texture& D3D12SampledTextureView::GetTexture() noexcept
    {
        return *texture_;
    }

    D3D12ColorAttachmentView::D3D12ColorAttachmentView(
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap,
        D3D12_CPU_DESCRIPTOR_HANDLE descriptor,
        D3D12Texture& texture) noexcept
        : descriptor_heap_(std::move(descriptor_heap)),
          descriptor_(descriptor),
          texture_(&texture)
    {
    }

    RHI::Texture& D3D12ColorAttachmentView::GetTexture() noexcept
    {
        return *texture_;
    }

    D3D12DepthStencilView::D3D12DepthStencilView(
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap,
        D3D12_CPU_DESCRIPTOR_HANDLE descriptor,
        D3D12Texture& texture) noexcept
        : descriptor_heap_(std::move(descriptor_heap)),
          descriptor_(descriptor),
          texture_(&texture)
    {
    }

    RHI::Texture& D3D12DepthStencilView::GetTexture() noexcept
    {
        return *texture_;
    }
}
