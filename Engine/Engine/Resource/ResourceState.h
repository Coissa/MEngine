#pragma once

#include <cstdint>

namespace GE::Resource
{
    enum class ResourceState : std::uint8_t
    {
        Unloaded,
        Loading,
        Ready,
        Failed
    };
}
