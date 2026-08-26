add_library(MEnginePlatformWindows STATIC
    Win32Application.cpp
    Win32Application.h
    Win32NativeWindowHandle.h
    Win32Window.cpp
    Win32Window.h
)

target_link_libraries(MEnginePlatformWindows
    PUBLIC
        MEnginePlatform
    PRIVATE
        MEngineCore
        user32
)

if(MSVC)
    target_compile_definitions(MEnginePlatformWindows
        PRIVATE
            UNICODE
            _UNICODE
    )

    target_compile_options(MEnginePlatformWindows
        PRIVATE
            /EHsc
            /utf-8
    )
endif()
