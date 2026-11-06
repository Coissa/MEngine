#include <Platform/WindowTypes.h>

#include <iostream>

int main()
{
    constexpr GE::Platform::WindowSize EmptySize;
    static_assert(!EmptySize.HasArea());

    constexpr GE::Platform::WindowSize WidthOnly{.width = 1280, .height = 0};
    static_assert(!WidthOnly.HasArea());

    constexpr GE::Platform::WindowSize HeightOnly{.width = 0, .height = 720};
    static_assert(!HeightOnly.HasArea());

    constexpr GE::Platform::WindowSize DrawableSize{.width = 1280, .height = 720};
    static_assert(DrawableSize.HasArea());

    const GE::Platform::WindowCreateInfo create_info;
    if (create_info.title != "MEngine" || create_info.client_size.width != 1280 ||
        create_info.client_size.height != 720 || !create_info.client_size.HasArea())
    {
        std::cerr << "[WindowTypesSmoke] Unexpected default window settings\n";
        return 1;
    }

    std::cout << "[WindowTypesSmoke] Passed\n";
    return 0;
}
