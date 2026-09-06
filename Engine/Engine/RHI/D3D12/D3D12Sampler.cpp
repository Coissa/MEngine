#include "D3D12Sampler.h"

#include <utility>

namespace GE::D3D12RHI
{
    D3D12Sampler::D3D12Sampler(
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap,
        D3D12_CPU_DESCRIPTOR_HANDLE descriptor,
        RHI::SamplerDescription description) noexcept
        : descriptor_heap_(std::move(descriptor_heap)),
          descriptor_(descriptor),
          description_(description)
    {
    }

    const RHI::SamplerDescription&
    D3D12Sampler::GetDescription() const noexcept
    {
        return description_;
    }
}
