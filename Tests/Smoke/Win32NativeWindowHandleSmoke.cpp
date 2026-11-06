#include <Platform/Windows/Win32NativeWindowHandle.h>

#include <iostream>

int main()
{
    constexpr GE::Platform::Windows::Win32NativeWindowHandle EmptyHandle;
    static_assert(!EmptyHandle.IsValid());

    int instance_storage = 0;
    int window_storage = 0;

    const GE::Platform::Windows::Win32NativeWindowHandle InstanceOnly{.instance = &instance_storage};
    if (InstanceOnly.IsValid())
    {
        std::cerr << "[Win32NativeWindowHandleSmoke] Instance-only handle is valid\n";
        return 1;
    }

    const GE::Platform::Windows::Win32NativeWindowHandle WindowOnly{.window = &window_storage};
    if (WindowOnly.IsValid())
    {
        std::cerr << "[Win32NativeWindowHandleSmoke] Window-only handle is valid\n";
        return 2;
    }

    const GE::Platform::Windows::Win32NativeWindowHandle CompleteHandle{.instance = &instance_storage,
                                                                        .window = &window_storage};
    if (!CompleteHandle.IsValid())
    {
        std::cerr << "[Win32NativeWindowHandleSmoke] Complete handle is invalid\n";
        return 3;
    }

    std::cout << "[Win32NativeWindowHandleSmoke] Passed\n";
    return 0;
}
