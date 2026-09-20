add_library(MEngineRenderResource STATIC
    GpuDrivenScene.cpp
    GpuDrivenScene.h
    GpuResourceRegistry.cpp
    GpuResourceRegistry.h
    RenderResourceUploader.cpp
    RenderResourceUploader.h
)

target_include_directories(MEngineRenderResource PUBLIC ${PROJECT_SOURCE_DIR}/Engine)
target_link_libraries(MEngineRenderResource
    PUBLIC
        MEngineAssetFormats
        MEngineCore
        MEngineRHI
)

if(MSVC)
    target_compile_options(MEngineRenderResource PRIVATE /EHsc /utf-8)
endif()
