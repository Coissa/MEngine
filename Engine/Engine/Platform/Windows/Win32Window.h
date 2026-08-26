#pragma once

#include "Engine/Platform/Windows/Win32NativeWindowHandle.h"
#include "Engine/Platform/WindowTypes.h"

#include <memory>

namespace GE::Platform::Windows
{
    class Win32Window final
    {
    public:
        [[nodiscard]] static std::unique_ptr<Win32Window> Create(const WindowCreateInfo& create_info);
        ~Win32Window() noexcept;

        Win32Window(const Win32Window&) = delete;
        Win32Window& operator=(const Win32Window&) = delete;
        Win32Window(Win32Window&&) = delete;
        Win32Window& operator=(Win32Window&&) = delete;

        void Show() noexcept;
        [[nodiscard]] bool PumpMessages() noexcept;
        [[nodiscard]] WindowSize GetClientSize() const noexcept;
        [[nodiscard]] Win32NativeWindowHandle GetNativeWindowHandle() const noexcept;

    private:
        class Impl;
        explicit Win32Window(std::unique_ptr<Impl> impl) noexcept;

        std::unique_ptr<Impl> m_impl;
    };
}
