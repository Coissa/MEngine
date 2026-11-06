#include <Platform/Windows/Win32Window.h>

int main()
{
    GE::Platform::WindowCreateInfo invalid_create_info;
    invalid_create_info.client_size = {};
    if (GE::Platform::Windows::Win32Window::Create(invalid_create_info) != nullptr)
    {
        return 1;
    }

    GE::Platform::WindowCreateInfo create_info;
    create_info.title = "MEngine \xE7\xAA\x97\xE5\x8F\xA3";
    create_info.client_size = {.width = 320, .height = 240};

    auto window = GE::Platform::Windows::Win32Window::Create(create_info);
    if (window == nullptr)
    {
        return 2;
    }
    if (!window->GetNativeWindowHandle().IsValid())
    {
        return 3;
    }

    const GE::Platform::WindowSize client_size = window->GetClientSize();
    if (client_size.width != 320 || client_size.height != 240)
    {
        return 4;
    }
    if (!window->PumpMessages())
    {
        return 5;
    }

    return 0;
}
