#include "Win32Application.h"
#include "Win32Window.h"

#include <memory>
#include <utility>

namespace GE::Platform::Windows
{
    Win32Application::Win32Application(
        std::unique_ptr<Win32Window> window) noexcept
        : m_window(std::move(window))
    {
    }

    Win32Application::~Win32Application() noexcept = default;

    [[nodiscard]] std::unique_ptr<Win32Application> Win32Application::Create(
        const WindowCreateInfo& create_info)
    {
        std::unique_ptr<Win32Window> window =
            Win32Window::Create(create_info);
        if (window == nullptr)
        {
            return nullptr;
        }

        return std::unique_ptr<Win32Application>(
            new Win32Application(std::move(window)));
    }

    void Win32Application::Show() noexcept
    {
        m_window->Show();
    }

    [[nodiscard]] bool Win32Application::PumpMessages() noexcept
    {
        return m_window->PumpMessages();
    }

    [[nodiscard]] WindowSize Win32Application::GetClientSize() const noexcept
    {
        return m_window->GetClientSize();
    }

    void Win32Application::SetMessageHandler(
        PlatformMessageHandler handler,
        void* context) noexcept
    {
        m_window->SetMessageHandler(handler, context);
    }

    [[nodiscard]] Win32NativeWindowHandle Win32Application::GetNativeWindowHandle() const noexcept
    {
        return m_window->GetNativeWindowHandle();
    }
}
