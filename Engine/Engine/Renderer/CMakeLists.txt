add_library(MEngineRenderer STATIC
    NullRenderer.cpp
    NullRenderer.h
    Private/ResourceStateTracker.cpp
    Private/ResourceStateTracker.h
    RHIRenderer.cpp
    RHIRenderer.h
    Renderer.h
)

target_compile_features(MEngineRenderer
    PUBLIC
        cxx_std_20
)

target_include_directories(MEngineRenderer
    PUBLIC
        ${PROJECT_SOURCE_DIR}/Engine
)

target_link_libraries(MEngineRenderer
    PUBLIC
        MEngineRenderCore
        MEngineRenderResource
        MEngineRHI
    PRIVATE
        MEngineCore
)

if(MSVC)
    target_compile_options(MEngineRenderer
        PRIVATE
            /EHsc
    )
endif()

# D3DCompile 只服务于示例资产准备，不进入通用 Renderer 或 D3D12 backend。
add_library(MEngineD3D12RendererExample STATIC
    Examples/D3D12RotatingCubeExample.cpp
    Examples/D3D12RotatingCubeExample.h
)

target_compile_features(MEngineD3D12RendererExample PUBLIC cxx_std_20)

target_include_directories(MEngineD3D12RendererExample
    PUBLIC
        ${PROJECT_SOURCE_DIR}/Engine
)

target_link_libraries(MEngineD3D12RendererExample
    PUBLIC
        MEngineRenderer
    PRIVATE
        d3dcompiler
)

if(MSVC)
    target_compile_options(MEngineD3D12RendererExample PRIVATE /EHsc /utf-8)
endif()
