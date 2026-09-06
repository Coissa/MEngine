#pragma once

#include <Engine/RHI/RHITextureView.h>

#include <d3d12.h>
#include <wrl/client.h>

namespace GE::D3D12RHI
{
    class D3D12CommandList;
    class D3D12Texture;

    // CPU SRV 描述符的所有者；ResourceSet 会把它复制到 shader-visible 堆。
    class D3D12SampledTextureView final : public RHI::SampledTextureView
    {
    public:
        D3D12SampledTextureView(
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap,
            D3D12_CPU_DESCRIPTOR_HANDLE descriptor,
            D3D12Texture& texture) noexcept;
        ~D3D12SampledTextureView() noexcept override = default;

        [[nodiscard]] RHI::Texture& GetTexture() noexcept override;

    private:
        friend class D3D12Device;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap_;
        D3D12_CPU_DESCRIPTOR_HANDLE descriptor_{};
        D3D12Texture* texture_ = nullptr;
    };

    // 一个 RTV 描述符及其堆；借用 Texture，调用方必须先销毁 View 再销毁/Resize Texture。
    class D3D12ColorAttachmentView final : public RHI::ColorAttachmentView
    {
    public:
        D3D12ColorAttachmentView(
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap,
            D3D12_CPU_DESCRIPTOR_HANDLE descriptor,
            D3D12Texture& texture) noexcept;
        ~D3D12ColorAttachmentView() noexcept override = default;

        [[nodiscard]] RHI::Texture& GetTexture() noexcept override;

    private:
        friend class D3D12CommandList;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap_;
        D3D12_CPU_DESCRIPTOR_HANDLE descriptor_{};
        D3D12Texture* texture_ = nullptr;
    };

    class D3D12DepthStencilView final : public RHI::DepthStencilView
    {
    public:
        D3D12DepthStencilView(
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap,
            D3D12_CPU_DESCRIPTOR_HANDLE descriptor,
            D3D12Texture& texture) noexcept;
        ~D3D12DepthStencilView() noexcept override = default;

        [[nodiscard]] RHI::Texture& GetTexture() noexcept override;

    private:
        friend class D3D12CommandList;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap_;
        D3D12_CPU_DESCRIPTOR_HANDLE descriptor_{};
        D3D12Texture* texture_ = nullptr;
    };
}
