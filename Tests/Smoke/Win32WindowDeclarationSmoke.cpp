#include <Platform/Windows/Win32Window.h>

#include <memory>
#include <type_traits>
#include <utility>

namespace
{
    using GE::Platform::WindowCreateInfo;
    using GE::Platform::WindowSize;
    using GE::Platform::Windows::Win32NativeWindowHandle;
    using GE::Platform::Windows::Win32Window;

    static_assert(!std::is_default_constructible_v<Win32Window>);
    static_assert(!std::is_copy_constructible_v<Win32Window>);
    static_assert(!std::is_copy_assignable_v<Win32Window>);
    static_assert(!std::is_move_constructible_v<Win32Window>);
    static_assert(!std::is_move_assignable_v<Win32Window>);
    static_assert(std::is_nothrow_destructible_v<Win32Window>);

    static_assert(std::is_same_v<decltype(Win32Window::Create(std::declval<const WindowCreateInfo&>())),
                                 std::unique_ptr<Win32Window>>);
    static_assert(noexcept(std::declval<Win32Window&>().Show()));
    static_assert(std::is_same_v<decltype(std::declval<Win32Window&>().PumpMessages()), bool>);
    static_assert(noexcept(std::declval<Win32Window&>().PumpMessages()));
    static_assert(std::is_same_v<decltype(std::declval<const Win32Window&>().GetClientSize()), WindowSize>);
    static_assert(noexcept(std::declval<const Win32Window&>().GetClientSize()));
    static_assert(
        std::is_same_v<decltype(std::declval<const Win32Window&>().GetNativeWindowHandle()), Win32NativeWindowHandle>);
    static_assert(noexcept(std::declval<const Win32Window&>().GetNativeWindowHandle()));
} // namespace

int main()
{
    return 0;
}
