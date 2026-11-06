#include "D3D12Resource.h"
#include "D3D12Device.h"
#include <cstring>

namespace GE::D3D12
{
    Buffer::Buffer(ID3D12Device& device, const RHI::BufferDesc& description) : desc(description)
    {
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = desc.memory == RHI::Memory::Upload     ? D3D12_HEAP_TYPE_UPLOAD
                    : desc.memory == RHI::Memory::Readback ? D3D12_HEAP_TYPE_READBACK
                                                           : D3D12_HEAP_TYPE_DEFAULT;
        heap.CreationNodeMask = heap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC resource{};
        resource.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resource.Width = desc.bytes;
        resource.Height = resource.DepthOrArraySize = resource.MipLevels = 1;
        resource.SampleDesc.Count = 1;
        resource.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (desc.storage && desc.memory == RHI::Memory::GPU)
            resource.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        tracked.fixedHeapState = desc.memory != RHI::Memory::GPU;
        tracked.decaysToCommon = !tracked.fixedHeapState;
        tracked.submittedState = desc.memory == RHI::Memory::Upload     ? D3D12_RESOURCE_STATE_GENERIC_READ
                                 : desc.memory == RHI::Memory::Readback ? D3D12_RESOURCE_STATE_COPY_DEST
                                                                        : D3D12_RESOURCE_STATE_COMMON;
        Check(device.CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &resource, tracked.submittedState, nullptr,
                                             IID_PPV_ARGS(&tracked.native)),
              "Create buffer");
        if (desc.memory == RHI::Memory::Upload)
        {
            const D3D12_RANGE noRead{0, 0};
            Check(tracked.native->Map(0, &noRead, reinterpret_cast<void**>(&mapped)), "Map upload buffer");
        }
    }

    Buffer::~Buffer()
    {
        if (mapped)
            tracked.native->Unmap(0, nullptr);
    }

    void Buffer::Write(size_t offset, const void* data, size_t bytes)
    {
        assert(mapped && offset + bytes <= desc.bytes);
        std::memcpy(mapped + offset, data, bytes);
    }

    Texture::Texture(ID3D12Device& device, const RHI::TextureDesc& description) : desc(description)
    {
        tracked.submittedSubresources.resize(desc.mipLevels * desc.arrayLayers, D3D12_RESOURCE_STATE_COMMON);
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        heap.CreationNodeMask = heap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC resource{};
        resource.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resource.Width = desc.width;
        resource.Height = desc.height;
        resource.DepthOrArraySize = UINT16(desc.arrayLayers);
        resource.MipLevels = UINT16(desc.mipLevels);
        resource.Format = desc.format == RHI::RHIFormat::D32_Float ? DXGI_FORMAT_R32_TYPELESS
                          : desc.allowSrgbViews                    ? DXGI_FORMAT_R8G8B8A8_TYPELESS
                                                                   : NativeFormat(desc.format);
        resource.SampleDesc.Count = 1;
        if (desc.renderTarget)
            resource.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (desc.depthStencil)
            resource.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        if (desc.storage)
            resource.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        D3D12_CLEAR_VALUE clear{};
        clear.Format = NativeFormat(desc.format);
        if (desc.depthStencil)
            clear.DepthStencil.Depth = 1.0f;
        else
        {
            clear.Color[0] = 0.025f;
            clear.Color[1] = 0.04f;
            clear.Color[2] = 0.07f;
            clear.Color[3] = 1.0f;
        }
        Check(device.CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &resource, D3D12_RESOURCE_STATE_COMMON,
                                             desc.renderTarget || desc.depthStencil ? &clear : nullptr,
                                             IID_PPV_ARGS(&tracked.native)),
              "Create texture");
    }

    Texture::Texture(ComPtr<ID3D12Resource> resource, const RHI::TextureDesc& description) : desc(description)
    {
        tracked.native = std::move(resource);
        tracked.submittedState = D3D12_RESOURCE_STATE_PRESENT;
    }

    std::unique_ptr<RHI::Buffer> Device::CreateBuffer(const RHI::BufferDesc& desc)
    {
        return std::make_unique<Buffer>(*native.Get(), desc);
    }

    std::unique_ptr<RHI::Texture> Device::CreateTexture(const RHI::TextureDesc& desc)
    {
        return std::make_unique<Texture>(*native.Get(), desc);
    }

    TimestampQuery::TimestampQuery(ID3D12Device& device, ID3D12CommandQueue& queue, uint32_t count)
    {
        D3D12_QUERY_HEAP_DESC queries{};
        queries.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        queries.Count = count;
        Check(device.CreateQueryHeap(&queries, IID_PPV_ARGS(&heap)), "Create timestamps");
        D3D12_HEAP_PROPERTIES properties{};
        properties.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = count * sizeof(uint64_t);
        desc.Height = desc.DepthOrArraySize = desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        Check(device.CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST,
                                             nullptr, IID_PPV_ARGS(&readback)),
              "Create timestamp readback");
        Check(readback->Map(0, nullptr, reinterpret_cast<void**>(&mapped)), "Map timestamps");
        Check(queue.GetTimestampFrequency(&frequency), "Timestamp frequency");
    }
    TimestampQuery::~TimestampQuery()
    {
        readback->Unmap(0, nullptr);
    }
    double TimestampQuery::Milliseconds(uint32_t first, uint32_t last) const
    {
        return double(mapped[last] - mapped[first]) * 1000.0 / double(frequency);
    }
    std::unique_ptr<RHI::TimestampQuery> Device::CreateTimestampQuery(uint32_t count)
    {
        return std::make_unique<TimestampQuery>(*native.Get(), *queue->Native(), count);
    }

    void Device::UploadBuffer(RHI::Buffer& destination, const void* data, size_t bytes)
    {
        auto staging = CreateBuffer({bytes, RHI::Memory::Upload});
        staging->Write(0, data, bytes);
        context->ExecuteImmediate(
            [&](RHI::CommandList& cmd)
            {
                cmd.Transition(destination, RHI::State::CopyDestination);
                cmd.CopyBuffer(*staging, destination, bytes);
            });
        // ExecuteImmediate waited. The local staging buffer can now be destroyed.
    }

    void Device::UploadTexture(RHI::Texture& destination, const void* data, uint32_t sourceRowPitch, uint32_t mip,
                               uint32_t layer)
    {
        auto& texture = static_cast<Texture&>(destination);
        auto nativeDesc = texture.tracked.native->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};
        uint32_t rows = 0;
        uint64_t rowBytes = 0, totalBytes = 0;
        const uint32_t subresource = mip + layer * texture.Description().mipLevels;
        native->GetCopyableFootprints(&nativeDesc, subresource, 1, 0, &layout, &rows, &rowBytes, &totalBytes);
        auto staging = CreateBuffer({size_t(totalBytes), RHI::Memory::Upload});
        for (uint32_t row = 0; row < rows; ++row)
            staging->Write(size_t(layout.Offset) + size_t(row) * layout.Footprint.RowPitch,
                           static_cast<const uint8_t*>(data) + size_t(row) * sourceRowPitch, size_t(rowBytes));
        context->ExecuteImmediate(
            [&](RHI::CommandList& cmd)
            {
                cmd.Transition(destination, RHI::State::CopyDestination, subresource);
                D3D12_TEXTURE_COPY_LOCATION source{};
                source.pResource = static_cast<Buffer&>(*staging).tracked.native.Get();
                source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                source.PlacedFootprint = layout;
                D3D12_TEXTURE_COPY_LOCATION target{};
                target.pResource = texture.tracked.native.Get();
                target.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                target.SubresourceIndex = subresource;
                static_cast<CommandList&>(cmd).Native()->CopyTextureRegion(&target, 0, 0, 0, &source, nullptr);
                cmd.Transition(destination, RHI::State::ShaderResource, subresource);
            });
    }

    RHI::Image Device::ReadbackTexture(RHI::Texture& sourceTexture)
    {
        auto& texture = static_cast<Texture&>(sourceTexture);
        assert(texture.Description().format == RHI::RHIFormat::R8G8B8A8_UNorm ||
               texture.Description().format == RHI::RHIFormat::B8G8R8A8_UNorm);
        const auto previous = texture.tracked.submittedState;
        auto nativeDesc = texture.tracked.native->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};
        uint64_t totalBytes = 0;
        native->GetCopyableFootprints(&nativeDesc, 0, 1, 0, &layout, nullptr, nullptr, &totalBytes);
        auto staging = CreateBuffer({size_t(totalBytes), RHI::Memory::Readback});
        auto* readback = static_cast<Buffer&>(*staging).tracked.native.Get();
        context->ExecuteImmediate(
            [&](RHI::CommandList& cmd)
            {
                cmd.Transition(sourceTexture, RHI::State::CopySource, 0);
                D3D12_TEXTURE_COPY_LOCATION source{};
                source.pResource = texture.tracked.native.Get();
                source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                D3D12_TEXTURE_COPY_LOCATION target{};
                target.pResource = readback;
                target.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                target.PlacedFootprint = layout;
                auto& nativeCmd = static_cast<CommandList&>(cmd);
                nativeCmd.Native()->CopyTextureRegion(&target, 0, 0, 0, &source, nullptr);
                nativeCmd.TransitionNative(texture.tracked, previous, 0);
            });
        RHI::Image image{texture.Description().width, texture.Description().height, {}};
        image.rgba.resize(size_t(image.width) * image.height * 4);
        void* mapped = nullptr;
        const D3D12_RANGE range{0, size_t(totalBytes)};
        Check(readback->Map(0, &range, &mapped), "Map readback");
        for (uint32_t row = 0; row < image.height; ++row)
            std::memcpy(image.rgba.data() + size_t(row) * image.width * 4,
                        static_cast<const uint8_t*>(mapped) + layout.Offset + size_t(row) * layout.Footprint.RowPitch,
                        size_t(image.width) * 4);
        const D3D12_RANGE noWrite{0, 0};
        readback->Unmap(0, &noWrite);
        if (texture.Description().format == RHI::RHIFormat::B8G8R8A8_UNorm)
            for (size_t i = 0; i < image.rgba.size(); i += 4)
                std::swap(image.rgba[i], image.rgba[i + 2]);
        return image;
    }
} // namespace GE::D3D12
