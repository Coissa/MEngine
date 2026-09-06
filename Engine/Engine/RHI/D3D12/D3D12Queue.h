#pragma once

#include <Engine/RHI/RHIQueue.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d3d12.h>
#include <wrl/client.h>

// 移除 SDK 的同名 A/W 宏，避免污染随后包含的 RHI 能力类型。
#ifdef DeviceCapabilities
#undef DeviceCapabilities
#endif

namespace GE::D3D12RHI
{
    class D3D12Device;

    // 后端私有的主图形队列封装；通用层只使用 RHI::Queue。
    class D3D12Queue final : public RHI::Queue
    {
    public:
        // 只接管已创建成功的 DIRECT 队列；调用方保证传入非空 COM 对象。
        explicit D3D12Queue(
            Microsoft::WRL::ComPtr<ID3D12CommandQueue> native_queue) noexcept;

        ~D3D12Queue() noexcept override = default;

        [[nodiscard]]
        RHI::QueueType GetType() const noexcept override;

        [[nodiscard]] RHI::RHIResult Signal(
            RHI::Fence& fence,
            std::uint64_t value) noexcept override;

        [[nodiscard]] RHI::RHIResult Execute(
            RHI::CommandList& command_list) noexcept override;

    private:
        friend class D3D12Device;

        // 随 Device 销毁而释放，不向 Renderer 暴露原生 GPU 对象。
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> native_queue_;

    };
}
