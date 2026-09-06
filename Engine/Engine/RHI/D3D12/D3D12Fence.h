#pragma once

#include <Engine/RHI/RHIFence.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>

namespace GE::D3D12RHI
{
    class D3D12Queue;

    // D3D12 时间线 Fence；只允许创建设备的主队列递增写入。
    class D3D12Fence final : public RHI::Fence
    {
    public:
        D3D12Fence(
            Microsoft::WRL::ComPtr<ID3D12Fence> native_fence,
            D3D12Queue& signal_queue,
            HANDLE completion_event) noexcept;

        ~D3D12Fence() noexcept override;

        [[nodiscard]] RHI::RHIResult GetCompletedValue(
            std::uint64_t& out_value) const noexcept override;

        [[nodiscard]] RHI::RHIResult Wait(
            std::uint64_t value,
            std::uint32_t timeout_ms) noexcept override;

    private:
        friend class D3D12Queue;

        // 持有原生 Fence 的 COM 引用。
        Microsoft::WRL::ComPtr<ID3D12Fence> native_fence_;

        // 标识唯一允许写入该 Fence 的队列，不拥有它。
        const D3D12Queue* signal_queue_ = nullptr;

        // CPU 的提交记录，不是 GPU 完成值。
        std::uint64_t last_signaled_value_ = 0;

        // Fence 独占并复用的自动重置事件；等待超时后也必须继续存活。
        HANDLE completion_event_ = nullptr;
    };
}
