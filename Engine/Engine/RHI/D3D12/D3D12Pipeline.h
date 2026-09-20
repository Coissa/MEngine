#pragma once

#include <Engine/RHI/RHIPipeline.h>

#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>
#include <vector>

namespace GE::D3D12RHI
{
    class D3D12CommandList;
    class D3D12Device;

    struct D3D12RootBinding final
    {
        std::uint32_t binding = 0;
        std::uint32_t root_parameter = 0;
        RHI::ResourceBindingType type = RHI::ResourceBindingType::ConstantBuffer;
    };

    class D3D12PipelineLayout final : public RHI::PipelineLayout
    {
    public:
        D3D12PipelineLayout(
            Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature,
            std::vector<D3D12RootBinding> bindings) noexcept;
        ~D3D12PipelineLayout() noexcept override = default;

    private:
        friend class D3D12CommandList;
        friend class D3D12Device;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        std::vector<D3D12RootBinding> bindings_;
    };

    struct D3D12BoundBuffer final
    {
        std::uint32_t root_parameter = 0;
        RHI::ResourceBindingType type = RHI::ResourceBindingType::ConstantBuffer;
        D3D12_GPU_VIRTUAL_ADDRESS address = 0;
    };

    struct D3D12BoundDescriptor final
    {
        std::uint32_t root_parameter = 0;
        D3D12_GPU_DESCRIPTOR_HANDLE descriptor{};
    };

    class D3D12ResourceSet final : public RHI::ResourceSet
    {
    public:
        D3D12ResourceSet(
            D3D12PipelineLayout& layout,
            std::vector<D3D12BoundBuffer> buffers,
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> resource_heap,
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> sampler_heap,
            std::vector<D3D12BoundDescriptor> descriptors) noexcept;
        ~D3D12ResourceSet() noexcept override = default;

    private:
        friend class D3D12CommandList;
        D3D12PipelineLayout* layout_ = nullptr;
        std::vector<D3D12BoundBuffer> buffers_;
        // ResourceSet 拥有 shader-visible 描述符副本，但不拥有底层纹理或 Sampler。
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> resource_heap_;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> sampler_heap_;
        std::vector<D3D12BoundDescriptor> descriptors_;
    };

    class D3D12GraphicsPipeline final : public RHI::GraphicsPipeline
    {
    public:
        D3D12GraphicsPipeline(
            Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state,
            Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature,
            D3D12PipelineLayout& layout,
            RHI::PrimitiveTopology topology) noexcept;
        ~D3D12GraphicsPipeline() noexcept override = default;

        [[nodiscard]] RHI::PrimitiveTopology
        GetTopology() const noexcept override;

    private:
        friend class D3D12CommandList;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state_;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        D3D12PipelineLayout* layout_ = nullptr;
        RHI::PrimitiveTopology topology_ = RHI::PrimitiveTopology::TriangleList;
    };

    class D3D12ComputePipeline final : public RHI::ComputePipeline
    {
    public:
        D3D12ComputePipeline(
            Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state,
            Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature,
            D3D12PipelineLayout& layout) noexcept;
        ~D3D12ComputePipeline() noexcept override = default;

    private:
        friend class D3D12CommandList;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state_;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        D3D12PipelineLayout* layout_ = nullptr;
    };
}
