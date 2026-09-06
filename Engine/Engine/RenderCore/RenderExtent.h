#pragma once

#include <cstdint>

namespace GE::RenderCore
{
    struct RenderExtent final
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;

        [[nodiscard]] constexpr bool HasArea() const noexcept
        {
            return width != 0 && height != 0;
        }
    };
}