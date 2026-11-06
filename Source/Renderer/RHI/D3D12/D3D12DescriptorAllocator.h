#pragma once
#include "D3D12Common.h"
#include <vector>

namespace GE::D3D12
{
    class DescriptorAllocator;

    class DescriptorAllocation
    {
    public:
        DescriptorAllocation() = default;
        DescriptorAllocation(DescriptorAllocator& allocator, uint32_t index);
        ~DescriptorAllocation();
        DescriptorAllocation(const DescriptorAllocation&) = delete;
        DescriptorAllocation& operator=(const DescriptorAllocation&) = delete;
        DescriptorAllocation(DescriptorAllocation&& other) noexcept;
        D3D12_CPU_DESCRIPTOR_HANDLE Cpu() const;
        D3D12_GPU_DESCRIPTOR_HANDLE Gpu() const;

    private:
        DescriptorAllocator* owner = nullptr;
        uint32_t index = 0;
    };

    class DescriptorAllocator
    {
    public:
        DescriptorAllocator(ID3D12Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity, bool visible);
        DescriptorAllocation Allocate();
        void Free(uint32_t index); // Caller guarantees GPU has finished using this slot.
        D3D12_CPU_DESCRIPTOR_HANDLE Cpu(uint32_t index) const;
        D3D12_GPU_DESCRIPTOR_HANDLE Gpu(uint32_t index) const;

        ID3D12DescriptorHeap* Native() const { return heap.Get(); }

    private:
        ComPtr<ID3D12DescriptorHeap> heap;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuStart{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuStart{};
        uint32_t stride;
        std::vector<uint32_t> freeSlots;
    };
} // namespace GE::D3D12
