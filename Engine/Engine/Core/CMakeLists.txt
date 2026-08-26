add_library(MEngineCore STATIC
    Diagnostics/Log.cpp
    Diagnostics/Log.h
    Time/FrameClock.cpp
    Time/FrameClock.h
)

target_include_directories(MEngineCore
    PUBLIC
        ${PROJECT_SOURCE_DIR}/Engine
)

if(MSVC)
    target_compile_options(MEngineCore
        PRIVATE
            /EHsc
    )
endif()
