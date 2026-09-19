add_library(MEngineCook STATIC
    CookedContainer.cpp
    CookedContainer.h
)

target_include_directories(MEngineCook
    PUBLIC
        ${PROJECT_SOURCE_DIR}/Engine
)

target_link_libraries(MEngineCook
    PUBLIC
        MEngineAsset
        MEngineFileSystem
)

if(MSVC)
    target_compile_options(MEngineCook PRIVATE /EHsc /utf-8)
endif()

add_library(MEngineCookAuthoring STATIC
    CookedContainerPublisher.cpp
    CookedContainerPublisher.h
)

target_include_directories(MEngineCookAuthoring
    PUBLIC
        ${PROJECT_SOURCE_DIR}/Engine
)

target_link_libraries(MEngineCookAuthoring
    PUBLIC
        MEngineCook
    PRIVATE
        MEngineFileSystemAuthoring
)

if(MSVC)
    target_compile_options(MEngineCookAuthoring PRIVATE /EHsc /utf-8)
endif()
