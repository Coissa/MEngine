#include "D3D12DescriptorAllocator.h"

namespace GE::D3D12
{
    DescriptorAllocation::DescriptorAllocation(DescriptorAllocator& allocator, uint32_t slot)
        : owner(&allocator), index(slot)
    {
    }

    DescriptorAllocation::~DescriptorAllocation()
    {
        if (owner)
            owner->Free(index);
    }

    DescriptorAllocation::DescriptorAllocation(DescriptorAllocation&& other) noexcept
        : owner(std::exchange(other.owner, nullptr)), index(other.index)
    {
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocation::Cpu() const
    {
        return owner->Cpu(index);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocation::Gpu() const
    {
        return owner->Gpu(index);
    }

    DescriptorAllocator::DescriptorAllocator(ID3D12Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity,
                                             bool visible)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = type;
        desc.NumDescriptors = capacity;
        desc.Flags = visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        Check(device.CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)), "CreateDescriptorHeap");
        stride = device.GetDescriptorHandleIncrementSize(type);
        cpuStart = heap->GetCPUDescriptorHandleForHeapStart();
        if (visible)
            gpuStart = heap->GetGPUDescriptorHandleForHeapStart();
        freeSlots.reserve(capacity);
        for (uint32_t i = capacity; i > 0; --i)
            freeSlots.push_back(i - 1);
    }

    DescriptorAllocation DescriptorAllocator::Allocate()
    {
        if (freeSlots.empty())
            throw std::runtime_error("Descriptor heap exhausted");
        const uint32_t index = freeSlots.back();
        freeSlots.pop_back();
        return DescriptorAllocation(*this, index);
    }

    void DescriptorAllocator::Free(uint32_t index)
    {
        freeSlots.push_back(index);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::Cpu(uint32_t index) const
    {
        auto handle = cpuStart;
        handle.ptr += SIZE_T(index) * stride;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocator::Gpu(uint32_t index) const
    {
        auto handle = gpuStart;
        handle.ptr += UINT64(index) * stride;
        return handle;
    }
} // namespace GE::D3D12
