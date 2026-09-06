#pragma once

#include <Engine/RHI/RHISampler.h>

#include <d3d12.h>
#include <wrl/client.h>

namespace GE::D3D12RHI
{
    class D3D12Device;

    // CPU Sampler 描述符的所有者；ResourceSet 会把描述符复制到可见堆。
    class D3D12Sampler final : public RHI::Sampler
    {
    public:
        D3D12Sampler(
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap,
            D3D12_CPU_DESCRIPTOR_HANDLE descriptor,
            RHI::SamplerDescription description) noexcept;
        ~D3D12Sampler() noexcept override = default;

        [[nodiscard]] const RHI::SamplerDescription&
        GetDescription() const noexcept override;

    private:
        friend class D3D12Device;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap_;
        D3D12_CPU_DESCRIPTOR_HANDLE descriptor_{};
        RHI::SamplerDescription description_;
    };
}
