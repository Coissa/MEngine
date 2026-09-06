#include "D3D12Fence.h"

#include <cstdio>
#include <limits>
#include <utility>

namespace GE::D3D12RHI
{
    D3D12Fence::D3D12Fence(
        Microsoft::WRL::ComPtr<ID3D12Fence> native_fence,
        D3D12Queue& signal_queue,
        HANDLE completion_event) noexcept
        : native_fence_(std::move(native_fence)),
          signal_queue_(&signal_queue),
          completion_event_(completion_event)
    {
    }

    D3D12Fence::~D3D12Fence() noexcept
    {
        if (completion_event_ != nullptr)
        {
            CloseHandle(completion_event_);
        }
    }

    RHI::RHIResult D3D12Fence::GetCompletedValue(
        std::uint64_t& out_value) const noexcept
    {
        out_value = 0;

        // 该 API 直接返回完成值；最大值代表设备已被移除。
        const std::uint64_t completed_value =
            native_fence_->GetCompletedValue();
        if (completed_value == std::numeric_limits<std::uint64_t>::max())
        {
            return RHI::RHIResult::BackendFailure;
        }

        out_value = completed_value;
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12Fence::Wait(
        std::uint64_t value,
        std::uint32_t timeout_ms) noexcept
    {
        // 只能等待已经提交给该 Fence 的值；最大值由 D3D12 保留作设备移除哨兵。
        if (value == std::numeric_limits<std::uint64_t>::max()
            || value > last_signaled_value_)
        {
            return RHI::RHIResult::InvalidArgument;
        }

        std::uint64_t completed_value = 0;
        RHI::RHIResult result = GetCompletedValue(completed_value);
        if (result != RHI::RHIResult::Success || completed_value >= value)
        {
            return result;
        }

        // 到达目标值时，D3D12 会把 Win32 自动重置事件置为有信号状态。
        const HRESULT event_result =
            native_fence_->SetEventOnCompletion(value, completion_event_);
        if (FAILED(event_result))
        {
            std::fprintf(
                stderr,
                "[D3D12RHI] Fence SetEventOnCompletion failed, "
                "HRESULT=0x%08lX\n",
                static_cast<unsigned long>(event_result));
            return RHI::RHIResult::BackendFailure;
        }

        const ULONGLONG start_tick = GetTickCount64();
        DWORD remaining_ms = timeout_ms;

        for (;;)
        {
            const DWORD wait_result =
                WaitForSingleObject(completion_event_, remaining_ms);

            if (wait_result == WAIT_FAILED)
            {
                std::fprintf(
                    stderr,
                    "[D3D12RHI] Fence WaitForSingleObject failed, "
                    "Win32Error=%lu\n",
                    static_cast<unsigned long>(GetLastError()));
                return RHI::RHIResult::BackendFailure;
            }

            // 无论事件被触发还是时间到，都重新读取 GPU 真值，避免边界竞态。
            result = GetCompletedValue(completed_value);
            if (result != RHI::RHIResult::Success)
            {
                return result;
            }
            if (completed_value >= value)
            {
                return RHI::RHIResult::Success;
            }
            if (wait_result == WAIT_TIMEOUT)
            {
                return RHI::RHIResult::Timeout;
            }
            if (wait_result != WAIT_OBJECT_0)
            {
                return RHI::RHIResult::BackendFailure;
            }

            // 先前一次超时留下的完成通知可能造成“旧信号”。保持本次注册，
            // 只用尚余预算继续等待，确保总等待时间不超过调用者给出的上限。
            const ULONGLONG elapsed_ms = GetTickCount64() - start_tick;
            if (elapsed_ms >= timeout_ms)
            {
                return RHI::RHIResult::Timeout;
            }
            remaining_ms = static_cast<DWORD>(timeout_ms - elapsed_ms);
        }
    }
}
