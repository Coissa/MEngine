add_library(MEngineLaunch STATIC
    EngineLoop.cpp
    EngineLoop.h
    EngineRunSettings.h
    ${PROJECT_SOURCE_DIR}/Engine/Engine/Core/Version.h
)

target_compile_options(MEngineLaunch
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/EHsc>
)

target_include_directories(MEngineLaunch
    PUBLIC
        ${PROJECT_SOURCE_DIR}/Engine
)

target_link_libraries(MEngineLaunch
    PUBLIC
        MEngineRenderCore
    PRIVATE
        MEngineAsset
        MEngineCore
        MEnginePlatform
        MEngineReflection
        MEngineRenderer
        MEngineRuntime
)
