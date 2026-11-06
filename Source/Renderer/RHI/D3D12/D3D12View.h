#pragma once
#include "D3D12DescriptorAllocator.h"
#include "D3D12Resource.h"

namespace GE::D3D12
{
    class Sampler final : public RHI::Sampler
    {
    public:
        Sampler(ID3D12Device& device, const RHI::SamplerDescription& desc, DescriptorAllocation allocation);
        DescriptorAllocation allocation;
    };
    class TextureView final : public RHI::TextureView
    {
    public:
        TextureView(ID3D12Device& device, Texture& resource, RHI::ViewKind kind, DescriptorAllocation allocation,
                    const RHI::SampledTextureViewDescription& desc = {});

        RHI::Texture& Resource() const override { return *texture; }

        RHI::ViewKind Kind() const override { return kind; }

        D3D12_CPU_DESCRIPTOR_HANDLE Cpu() const { return allocation.Cpu(); }

        D3D12_GPU_DESCRIPTOR_HANDLE Gpu() const { return allocation.Gpu(); }

    private:
        Texture* texture; // Non-owning. Resource group destroys views before textures.
        RHI::ViewKind kind;
        DescriptorAllocation allocation;
    };
} // namespace GE::D3D12
