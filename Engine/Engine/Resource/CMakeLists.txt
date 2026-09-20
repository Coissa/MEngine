add_library(MEngineResource STATIC
    ResourceHandle.h
    ResourceManager.cpp
    ResourceManager.h
    ResourceState.h
)

target_include_directories(MEngineResource PUBLIC ${PROJECT_SOURCE_DIR}/Engine)
target_link_libraries(MEngineResource
    PUBLIC
        MEngineAsset
        MEngineAssetFormats
        MEngineCook
        MEngineCore
        MEngineFileSystem
)

if(MSVC)
    target_compile_options(MEngineResource PRIVATE /EHsc /utf-8)
endif()
