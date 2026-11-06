#include <Platform/Windows/Win32Application.h>

#include <memory>
#include <type_traits>

namespace
{
    using GE::Platform::Windows::Win32Application;

    static_assert(std::is_base_of_v<GE::Platform::PlatformApplication, Win32Application>);
    static_assert(!std::is_copy_constructible_v<Win32Application>);
    static_assert(!std::is_move_constructible_v<Win32Application>);
    static_assert(std::is_nothrow_destructible_v<Win32Application>);
} // namespace

int main()
{
    GE::Platform::WindowCreateInfo invalid_create_info;
    invalid_create_info.client_size = {};
    if (Win32Application::Create(invalid_create_info) != nullptr)
    {
        return 1;
    }

    GE::Platform::WindowCreateInfo create_info;
    create_info.title = "MEngine Application Smoke";
    create_info.client_size = {.width = 320, .height = 240};

    std::unique_ptr<Win32Application> application = Win32Application::Create(create_info);
    if (application == nullptr)
    {
        return 2;
    }
    if (!application->GetNativeWindowHandle().IsValid())
    {
        return 3;
    }

    const GE::Platform::PlatformApplication& platform_application = *application;
    const GE::Platform::WindowSize client_size = platform_application.GetClientSize();
    if (client_size.width != 320 || client_size.height != 240)
    {
        return 4;
    }
    if (!application->PumpMessages())
    {
        return 5;
    }

    return 0;
}
