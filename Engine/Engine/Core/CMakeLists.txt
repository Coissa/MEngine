add_library(MEngineCore STATIC
    Diagnostics/Log.cpp
    Diagnostics/Log.h
    Math/angle.h
    Math/conventions.h
    Math/geometry.h
    Math/math.h
    Math/matrix4.h
    Math/quaternion.h
    Math/vector.h
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
