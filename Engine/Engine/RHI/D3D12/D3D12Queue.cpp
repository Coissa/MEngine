#include "D3D12Queue.h"
#include "D3D12Fence.h"
#include "D3D12Command.h"

#include <cstdio>
#include <limits>
#include <utility>

namespace GE::D3D12RHI
{
    D3D12Queue::D3D12Queue(
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> native_queue) noexcept
        : native_queue_(std::move(native_queue))
    {
    }

    RHI::QueueType D3D12Queue::GetType() const noexcept
    {
        // 本检查点只创建 DIRECT 主队列，不虚报额外的独立计算或复制队列。
        return RHI::QueueType::Graphics;
    }

    RHI::RHIResult D3D12Queue::Signal(
        RHI::Fence& fence,
        std::uint64_t value) noexcept
    {
        auto* d3d12_fence = dynamic_cast<D3D12Fence*>(&fence);
        if (d3d12_fence == nullptr
            || d3d12_fence->signal_queue_ != this
            || value <= d3d12_fence->last_signaled_value_
            || value == std::numeric_limits<std::uint64_t>::max())
        {
            return RHI::RHIResult::InvalidArgument;
        }

        // GPU 侧 Signal：成功只表示标记已进入队列，不表示 GPU 已执行到这里。
        const HRESULT signal_result = native_queue_->Signal(
            d3d12_fence->native_fence_.Get(),
            value);
        if (FAILED(signal_result))
        {
            std::fprintf(
                stderr,
                "[D3D12RHI] Queue Signal failed, HRESULT=0x%08lX\n",
                static_cast<unsigned long>(signal_result));
            return RHI::RHIResult::BackendFailure;
        }

        d3d12_fence->last_signaled_value_ = value;
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12Queue::Execute(
        RHI::CommandList& command_list) noexcept
    {
        auto* native_list = dynamic_cast<D3D12CommandList*>(&command_list);
        if (native_list == nullptr || native_list->recording_)
        {
            return RHI::RHIResult::InvalidArgument;
        }
        ID3D12CommandList* lists[] = {native_list->native_list_.Get()};
        native_queue_->ExecuteCommandLists(1, lists);
        return RHI::RHIResult::Success;
    }
}
