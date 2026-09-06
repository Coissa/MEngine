#include "D3D12Buffer.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace GE::D3D12RHI
{
    D3D12Buffer::D3D12Buffer(
        Microsoft::WRL::ComPtr<ID3D12Resource> native_resource,
        RHI::BufferDescription description) noexcept
        : native_resource_(std::move(native_resource)),
          description_(std::move(description))
    {
    }

    const RHI::BufferDescription&
    D3D12Buffer::GetDescription() const noexcept
    {
        return description_;
    }

    RHI::RHIResult D3D12Buffer::Map(
        std::uint64_t offset,
        std::uint64_t size,
        void*& out_data) noexcept
    {
        out_data = nullptr;
        if (description_.memory != RHI::MemoryType::Upload
            || mapped_ || size == 0 || offset > description_.size
            || size > description_.size - offset)
        {
            return RHI::RHIResult::InvalidArgument;
        }

        // Upload Heap 只供 CPU 写入，空读取区间告诉驱动 CPU 不会读取旧内容。
        const D3D12_RANGE read_range{0, 0};
        void* base = nullptr;
        const HRESULT result = native_resource_->Map(0, &read_range, &base);
        if (FAILED(result))
        {
            return RHI::RHIResult::BackendFailure;
        }
        mapped_ = true;
        out_data = static_cast<std::byte*>(base) + offset;
        return RHI::RHIResult::Success;
    }

    void D3D12Buffer::Unmap(
        std::uint64_t offset,
        std::uint64_t size) noexcept
    {
        if (!mapped_)
        {
            return;
        }
        const std::uint64_t begin = (std::min)(offset, description_.size);
        const std::uint64_t end =
            begin == offset && size <= description_.size - begin
            ? begin + size
            : description_.size;
        const D3D12_RANGE written_range{
            static_cast<SIZE_T>(begin),
            static_cast<SIZE_T>(end)};
        native_resource_->Unmap(0, &written_range);
        mapped_ = false;
    }
}
