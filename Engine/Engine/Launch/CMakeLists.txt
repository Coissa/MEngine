add_library(MEngineLaunch STATIC
    EngineLoop.cpp
    EngineLoop.h
    ${PROJECT_SOURCE_DIR}/Engine/Engine/Core/Version.h
)

target_include_directories(MEngineLaunch
    PUBLIC
        ${PROJECT_SOURCE_DIR}/Engine
)

target_link_libraries(MEngineLaunch
    PRIVATE
        MEngineReflection
        MEngineRuntime
)
