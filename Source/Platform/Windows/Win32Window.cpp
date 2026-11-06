#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Win32Window.h"
#include "Core/Diagnostics/Log.h"

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <memory>

namespace GE::Platform::Windows
{
    using GE::Core::LogLevel;
    using GE::Core::LogMessage;

    namespace
    {
        constexpr wchar_t WindowClassName[] = L"MEngine.Win32Window";

        [[nodiscard]] std::optional<std::wstring> Utf8ToWide(std::string_view utf8)
        {
            if (utf8.empty())
            {
                return std::wstring{};
            }

            if (utf8.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            {
                return std::nullopt;
            }

            const int utf8_size = static_cast<int>(utf8.size());
            const int utf16_size =
                ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), utf8_size, nullptr, 0);
            if (utf16_size == 0)
            {
                return std::nullopt;
            }

            std::wstring wide_string(static_cast<std::size_t>(utf16_size), L'\0');
            const int converted_size = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), utf8_size,
                                                             wide_string.data(), utf16_size);
            if (converted_size != utf16_size)
            {
                return std::nullopt;
            }

            return wide_string;
        }
    } // namespace

    class Win32Window::Impl
    {
    public:
        HINSTANCE instance = nullptr;
        HWND window = nullptr;
        ATOM window_class_atom = 0;
        WindowSize client_size;
        bool close_requested = false;
        PlatformMessageHandler message_handler = nullptr;
        void* message_handler_context = nullptr;

        static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM word_parameter,
                                                LPARAM long_parameter) noexcept;
    };

    LRESULT CALLBACK Win32Window::Impl::WindowProcedure(HWND window, UINT message, WPARAM word_parameter,
                                                        LPARAM long_parameter) noexcept
    {
        if (message == WM_NCCREATE)
        {
            const auto* create_structure = reinterpret_cast<const CREATESTRUCTW*>(long_parameter);
            if (create_structure == nullptr)
            {
                return FALSE;
            }

            auto* impl = static_cast<Impl*>(create_structure->lpCreateParams);
            if (impl == nullptr)
            {
                return FALSE;
            }

            ::SetLastError(ERROR_SUCCESS);
            const LONG_PTR previous_value =
                ::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl));
            if (previous_value == 0 && ::GetLastError() != ERROR_SUCCESS)
            {
                return FALSE;
            }

            impl->window = window;
            return TRUE;
        }

        auto* impl = reinterpret_cast<Impl*>(::GetWindowLongPtrW(window, GWLP_USERDATA));
        if (impl == nullptr)
        {
            return ::DefWindowProcW(window, message, word_parameter, long_parameter);
        }

        if (impl->message_handler != nullptr &&
            impl->message_handler(impl->message_handler_context,
                                  {.native_window = reinterpret_cast<std::uintptr_t>(window),
                                   .message = message,
                                   .word_parameter = word_parameter,
                                   .long_parameter = long_parameter}))
        {
            return 1;
        }

        switch (message)
        {
        case WM_CLOSE:
            impl->close_requested = true;
            return 0;

        case WM_SIZE:
            if (word_parameter == SIZE_MINIMIZED)
            {
                impl->client_size = {};
            }
            else
            {
                impl->client_size = {.width = static_cast<std::uint32_t>(LOWORD(long_parameter)),
                                     .height = static_cast<std::uint32_t>(HIWORD(long_parameter))};
            }
            return 0;

        case WM_NCDESTROY:
            impl->window = nullptr;
            static_cast<void>(::SetWindowLongPtrW(window, GWLP_USERDATA, 0));
            return ::DefWindowProcW(window, message, word_parameter, long_parameter);

        default:
            return ::DefWindowProcW(window, message, word_parameter, long_parameter);
        }
    }

    Win32Window::Win32Window(std::unique_ptr<Impl> impl) noexcept : m_impl(std::move(impl)) {}

    Win32Window::~Win32Window() noexcept
    {
        if (m_impl == nullptr)
        {
            return;
        }

        if (m_impl->window != nullptr)
        {
            static_cast<void>(::DestroyWindow(m_impl->window));
            m_impl->window = nullptr;
        }

        if (m_impl->window_class_atom != 0 && m_impl->instance != nullptr)
        {
            static_cast<void>(::UnregisterClassW(WindowClassName, m_impl->instance));
            m_impl->window_class_atom = 0;
        }
    }

    [[nodiscard]] std::unique_ptr<Win32Window> Win32Window::Create(const WindowCreateInfo& create_info)
    {
        if (!create_info.client_size.HasArea())
        {
            LogMessage(LogLevel::Error, "PlatformWindows", "Cilent has not Area");
            return nullptr;
        }
        if (create_info.client_size.height > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
            create_info.client_size.width > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
        {
            LogMessage(LogLevel::Error, "PlatformWindows", "Cilent size out");
            return nullptr;
        }
        HINSTANCE winStance = GetModuleHandleW(nullptr);
        if (winStance == nullptr)
        {
            LogMessage(LogLevel::Error, "PlatformWindows", "GetModuleHandleW Failed");
            return nullptr;
        }
        std::optional<std::wstring> title = Utf8ToWide(create_info.title);
        if (!title.has_value())
        {
            LogMessage(LogLevel::Error, "PlatformWindows", "Window title is not valid UTF-8");
            return nullptr;
        }

        std::unique_ptr<Impl> impl = std::make_unique<Impl>();
        impl->instance = winStance;
        Impl* impl_pointer = impl.get();
        std::unique_ptr<Win32Window> win(new Win32Window(std::move(impl)));

        // 窗口类描述
        WNDCLASSEXW winclass{};
        winclass.cbSize = sizeof(WNDCLASSEXW);
        winclass.style = CS_HREDRAW | CS_VREDRAW;      // 宽高变化重绘
        winclass.lpfnWndProc = &Impl::WindowProcedure; // 窗口发射消息所调用函数址
        winclass.hInstance = winStance;
        winclass.hCursor = LoadCursorW(nullptr, IDC_ARROW); // 光标
        winclass.lpszClassName = WindowClassName;

        if (winclass.hCursor == nullptr)
        {
            LogMessage(LogLevel::Error, "PlatformWindows", "LoadCursorW Failed");
            return nullptr;
        }

        ATOM window_class_atom = RegisterClassExW(&winclass);
        if (window_class_atom == 0)
        {
            LogMessage(LogLevel::Error, "PlatformWindows", "Register Window Class Failed");
            return nullptr;
        }
        impl_pointer->window_class_atom = window_class_atom;

        RECT rect;
        rect.left = 0;
        rect.top = 0;
        rect.right = static_cast<LONG>(create_info.client_size.width);
        rect.bottom = static_cast<LONG>(create_info.client_size.height);

        if (!AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0))
        {
            LogMessage(LogLevel::Error, "PlatformWindows", "AdjustWindowRectEx Failed");
            return nullptr;
        }

        HWND hwnd =
            CreateWindowExW(0, WindowClassName, title->c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                            rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, winStance, impl_pointer);
        if (hwnd == nullptr)
        {
            LogMessage(LogLevel::Error, "PlatformWindows", "CreateWindowExW Failed");
            return nullptr;
        }
        impl_pointer->window = hwnd;
        if (!GetClientRect(hwnd, &rect))
        {
            LogMessage(LogLevel::Error, "PlatformWindows", "Can not get Client Rect");
            return nullptr;
        }
        impl_pointer->client_size = {.width = static_cast<std::uint32_t>(rect.right - rect.left),
                                     .height = static_cast<std::uint32_t>(rect.bottom - rect.top)};
        return win;
    }

    void Win32Window::Show() noexcept
    {
        if (m_impl == nullptr || m_impl->window == nullptr)
        {
            return;
        }

        static_cast<void>(::ShowWindow(m_impl->window, SW_SHOWDEFAULT));
        static_cast<void>(::UpdateWindow(m_impl->window));
    }

    [[nodiscard]] bool Win32Window::PumpMessages() noexcept
    {
        if (m_impl == nullptr || m_impl->window == nullptr)
        {
            return false;
        }

        MSG message{};
        while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                m_impl->close_requested = true;
                continue;
            }

            static_cast<void>(::TranslateMessage(&message));
            static_cast<void>(::DispatchMessageW(&message));
        }

        return !m_impl->close_requested;
    }

    [[nodiscard]] WindowSize Win32Window::GetClientSize() const noexcept
    {
        if (m_impl == nullptr)
        {
            return {};
        }

        return m_impl->client_size;
    }

    [[nodiscard]] Win32NativeWindowHandle Win32Window::GetNativeWindowHandle() const noexcept
    {
        if (m_impl == nullptr)
        {
            return {};
        }

        return {.instance = m_impl->instance, .window = m_impl->window};
    }

    void Win32Window::SetMessageHandler(PlatformMessageHandler handler, void* context) noexcept
    {
        if (m_impl == nullptr)
        {
            return;
        }
        m_impl->message_handler = handler;
        m_impl->message_handler_context = context;
    }

} // namespace GE::Platform::Windows
