#pragma once
#include "../RHIResource.h"
#include "D3D12Common.h"

namespace GE::D3D12
{
    // Queue-known state, not a query of the GPU's current physical state.
    struct TrackedResource
    {
        ComPtr<ID3D12Resource> native;
        D3D12_RESOURCE_STATES submittedState = D3D12_RESOURCE_STATE_COMMON;
        std::vector<D3D12_RESOURCE_STATES> submittedSubresources;
        bool decaysToCommon = false;
        bool fixedHeapState = false;
    };

    class Buffer final : public RHI::Buffer
    {
    public:
        Buffer(ID3D12Device& device, const RHI::BufferDesc& desc);
        ~Buffer() override;

        const RHI::BufferDesc& Description() const override { return desc; }

        void Write(size_t offset, const void* data, size_t bytes) override;
        TrackedResource tracked;

    private:
        RHI::BufferDesc desc;
        uint8_t* mapped = nullptr;
    };

    class TimestampQuery final : public RHI::TimestampQuery
    {
    public:
        TimestampQuery(ID3D12Device&, ID3D12CommandQueue&, uint32_t count);
        ~TimestampQuery() override;
        double Milliseconds(uint32_t first, uint32_t last) const override;
        ComPtr<ID3D12QueryHeap> heap;
        ComPtr<ID3D12Resource> readback;

    private:
        uint64_t* mapped = nullptr;
        uint64_t frequency = 0;
    };

    class Texture final : public RHI::Texture
    {
    public:
        Texture(ID3D12Device& device, const RHI::TextureDesc& desc);
        Texture(ComPtr<ID3D12Resource> native, const RHI::TextureDesc& desc);

        const RHI::TextureDesc& Description() const override { return desc; }

        TrackedResource tracked;

    private:
        RHI::TextureDesc desc;
    };
} // namespace GE::D3D12
