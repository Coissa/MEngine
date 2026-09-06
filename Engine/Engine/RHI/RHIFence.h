#pragma once

#include "RHIResult.h"

#include <cstdint>

namespace GE::RHI
{
    class Fence
    {
    public:
        virtual ~Fence() noexcept = default;

        Fence(const Fence&) = delete;
        Fence& operator=(const Fence&) = delete;
        Fence(Fence&&) = delete;
        Fence& operator=(Fence&&) = delete;

        // 查询 GPU 实际完成进度；失败时输出必须重置为 0。
        [[nodiscard]] virtual RHIResult GetCompletedValue(
            std::uint64_t& out_value) const noexcept = 0;

        // 最多等待 timeout_ms 毫秒，直到 GPU 完成到 value。
        // 0 毫秒表示只检查一次；超时返回 Timeout，不把正常的 GPU 忙碌误报为故障。
        [[nodiscard]] virtual RHIResult Wait(
            std::uint64_t value,
            std::uint32_t timeout_ms) noexcept = 0;

    protected:
        Fence() noexcept = default;
    };
}
