#pragma once

namespace GE::Platform::Windows
{
    struct Win32NativeWindowHandle final
    {
        void* instance = nullptr;
        void* window = nullptr;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return instance != nullptr && window != nullptr;
        }

    };
}
