#pragma once

#include "Engine/Platform/PlatformApplication.h"
#include "Engine/Platform/WindowTypes.h"
#include "Engine/Platform/Windows/Win32NativeWindowHandle.h"

#include <memory>

namespace GE::Platform::Windows
{

    class Win32Window;

    class Win32Application final : public GE::Platform::PlatformApplication
    {
    public:
        [[nodiscard]] static std::unique_ptr<Win32Application> Create(
            const WindowCreateInfo& create_info);

        ~Win32Application() noexcept override;

        Win32Application(const Win32Application&) = delete;
        Win32Application& operator=(const Win32Application&) = delete;
        Win32Application(Win32Application&&) = delete;
        Win32Application& operator=(Win32Application&&) = delete;

        void Show() noexcept;

        [[nodiscard]] bool PumpMessages() noexcept override;

        [[nodiscard]] WindowSize GetClientSize() const noexcept;

        [[nodiscard]] Win32NativeWindowHandle GetNativeWindowHandle() const noexcept;

    private:
        explicit Win32Application(std::unique_ptr<Win32Window> window) noexcept;

        std::unique_ptr<Win32Window> m_window;
    };
}
