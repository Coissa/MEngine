#include "D3D12Pipeline.h"

#include <utility>

namespace GE::D3D12RHI
{
    D3D12PipelineLayout::D3D12PipelineLayout(
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature,
        std::vector<D3D12RootBinding> bindings) noexcept
        : root_signature_(std::move(root_signature)),
          bindings_(std::move(bindings))
    {
    }

    D3D12ResourceSet::D3D12ResourceSet(
        D3D12PipelineLayout& layout,
        std::vector<D3D12BoundBuffer> buffers,
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> resource_heap,
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> sampler_heap,
        std::vector<D3D12BoundDescriptor> descriptors) noexcept
        : layout_(&layout),
          buffers_(std::move(buffers)),
          resource_heap_(std::move(resource_heap)),
          sampler_heap_(std::move(sampler_heap)),
          descriptors_(std::move(descriptors))
    {
    }

    D3D12GraphicsPipeline::D3D12GraphicsPipeline(
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state,
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature,
        D3D12PipelineLayout& layout,
        RHI::PrimitiveTopology topology) noexcept
        : pipeline_state_(std::move(pipeline_state)),
          root_signature_(std::move(root_signature)),
          layout_(&layout),
          topology_(topology)
    {
    }

    RHI::PrimitiveTopology
    D3D12GraphicsPipeline::GetTopology() const noexcept
    {
        return topology_;
    }
}
