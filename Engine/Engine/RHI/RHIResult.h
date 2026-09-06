#pragma once

#include <cstdint>

namespace GE::RHI
{
    enum class RHIResult : std::uint8_t
    {
        Success,
        Unsupported,
        InvalidArgument,
        BackendFailure,

        // 操作在调用者给出的有限等待时间内没有完成；它不是后端故障。
        Timeout
    };
}
