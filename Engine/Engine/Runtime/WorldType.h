#pragma once

#include <cstdint>

namespace GE::Runtime
{
    enum class WorldType : std::uint8_t
    {
        None,
        Game,
        Editor,
        PIE
    };
}
