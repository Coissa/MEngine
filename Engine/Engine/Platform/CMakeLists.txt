add_library(MEnginePlatform INTERFACE)

target_sources(MEnginePlatform
    INTERFACE
        PlatformApplication.h
        WindowTypes.h
)

target_compile_features(MEnginePlatform
    INTERFACE
        cxx_std_20
)

target_include_directories(MEnginePlatform
    INTERFACE
        ${PROJECT_SOURCE_DIR}/Engine
)

add_subdirectory(Windows)
