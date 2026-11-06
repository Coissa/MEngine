#include "D3D12View.h"
#include "D3D12Device.h"

namespace GE::D3D12
{
    TextureView::TextureView(ID3D12Device& device, Texture& resource, RHI::ViewKind type,
                             DescriptorAllocation descriptor, const RHI::SampledTextureViewDescription& view)
        : texture(&resource), kind(type), allocation(std::move(descriptor))
    {
        const auto& source = resource.Description();
        const uint32_t layers =
            view.array_layer_count ? view.array_layer_count : source.arrayLayers - view.base_array_layer;
        const bool array = source.arrayLayers > 1;
        if (kind == RHI::ViewKind::RenderTarget)
        {
            D3D12_RENDER_TARGET_VIEW_DESC desc{};
            desc.Format = NativeFormat(source.format);
            desc.ViewDimension = array ? D3D12_RTV_DIMENSION_TEXTURE2DARRAY : D3D12_RTV_DIMENSION_TEXTURE2D;
            if (array)
                desc.Texture2DArray = {view.base_mip_level, view.base_array_layer, layers, 0};
            else
                desc.Texture2D.MipSlice = view.base_mip_level;
            device.CreateRenderTargetView(resource.tracked.native.Get(), &desc, Cpu());
        }
        else if (kind == RHI::ViewKind::DepthStencil)
        {
            D3D12_DEPTH_STENCIL_VIEW_DESC desc{};
            desc.Format = NativeFormat(source.format);
            desc.ViewDimension = array ? D3D12_DSV_DIMENSION_TEXTURE2DARRAY : D3D12_DSV_DIMENSION_TEXTURE2D;
            if (array)
                desc.Texture2DArray = {view.base_mip_level, view.base_array_layer, layers};
            else
                desc.Texture2D.MipSlice = view.base_mip_level;
            device.CreateDepthStencilView(resource.tracked.native.Get(), &desc, Cpu());
        }
        else if (kind == RHI::ViewKind::Storage)
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
            desc.Format = NativeFormat(source.format);
            desc.ViewDimension = array ? D3D12_UAV_DIMENSION_TEXTURE2DARRAY : D3D12_UAV_DIMENSION_TEXTURE2D;
            if (array)
                desc.Texture2DArray = {view.base_mip_level, view.base_array_layer, layers, 0};
            else
                desc.Texture2D.MipSlice = view.base_mip_level;
            device.CreateUnorderedAccessView(resource.tracked.native.Get(), nullptr, &desc, Cpu());
        }
        else
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
            const uint32_t mips = view.mip_level_count ? view.mip_level_count : source.mipLevels - view.base_mip_level;
            desc.Format = source.format == RHI::RHIFormat::D32_Float ? DXGI_FORMAT_R32_FLOAT
                          : view.srgb                                ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                                                                     : NativeFormat(source.format);
            desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            if (view.dimension == RHI::SampledTextureViewDimension::TextureCube)
            {
                desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                desc.TextureCube.MostDetailedMip = view.base_mip_level;
                desc.TextureCube.MipLevels = mips;
            }
            else if (array)
            {
                desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray = {view.base_mip_level, mips, view.base_array_layer, layers, 0, 0};
            }
            else
            {
                desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                desc.Texture2D = {view.base_mip_level, mips, 0, 0};
            }
            device.CreateShaderResourceView(resource.tracked.native.Get(), &desc, Cpu());
        }
    }

    std::unique_ptr<RHI::TextureView> Device::CreateView(RHI::Texture& resource, RHI::ViewKind kind,
                                                         const RHI::SampledTextureViewDescription& view)
    {
        auto& texture = static_cast<Texture&>(resource);
        auto& allocator = kind == RHI::ViewKind::RenderTarget   ? *rtvDescriptors
                          : kind == RHI::ViewKind::DepthStencil ? *dsvDescriptors
                                                                : *shaderDescriptors;
        return std::make_unique<TextureView>(*native.Get(), texture, kind, allocator.Allocate(), view);
    }

    Sampler::Sampler(ID3D12Device& device, const RHI::SamplerDescription& description, DescriptorAllocation descriptor)
        : allocation(std::move(descriptor))
    {
        const auto address = [](RHI::SamplerAddressMode m)
        {
            return m == RHI::SamplerAddressMode::Repeat ? D3D12_TEXTURE_ADDRESS_MODE_WRAP
                                                        : D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        };
        D3D12_SAMPLER_DESC desc{};
        desc.Filter = description.filter == RHI::SamplerFilter::Nearest ? D3D12_FILTER_MIN_MAG_MIP_POINT
                                                                        : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = address(description.address_u);
        desc.AddressV = address(description.address_v);
        desc.AddressW = address(description.address_w);
        desc.MaxAnisotropy = 1;
        desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        desc.MaxLOD = D3D12_FLOAT32_MAX;
        device.CreateSampler(&desc, allocation.Cpu());
    }
    std::unique_ptr<RHI::Sampler> Device::CreateSampler(const RHI::SamplerDescription& desc)
    {
        return std::make_unique<Sampler>(*native.Get(), desc, samplerDescriptors->Allocate());
    }
} // namespace GE::D3D12
