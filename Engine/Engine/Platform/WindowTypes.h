#pragma once

#include <cstdint>
#include <string>

namespace GE::Platform
{
    struct WindowSize final
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;

        [[nodiscard]] constexpr bool HasArea() const noexcept
        {
            return width != 0 && height != 0;
        }
    };

    struct WindowCreateInfo final
    {
        std::string title = "MEngine";
        WindowSize client_size{
            .width = 1280,
            .height = 720
        };
    };
}
