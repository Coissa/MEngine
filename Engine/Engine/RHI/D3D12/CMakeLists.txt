add_library(MEngineD3D12RHI STATIC
    D3D12Backend.cpp
    D3D12Buffer.cpp
    D3D12Device.cpp
    D3D12Fence.cpp
    D3D12Pipeline.cpp
    D3D12Queue.cpp
    D3D12Sampler.cpp
    D3D12Swapchain.cpp
    D3D12Shader.cpp
    D3D12Texture.cpp
    D3D12TextureView.cpp
    D3D12Command.cpp
)

target_sources(MEngineD3D12RHI
    PUBLIC
        D3D12BackendFactory.h
    PRIVATE
        D3D12Backend.h
        D3D12Buffer.h
        D3D12Device.h
        D3D12Fence.h
        D3D12Pipeline.h
        D3D12Queue.h
        D3D12Sampler.h
        D3D12Swapchain.h
        D3D12Shader.h
        D3D12Texture.h
        D3D12TextureView.h
        D3D12Command.h
)

target_compile_features(MEngineD3D12RHI
    PUBLIC
        cxx_std_20
)

# 仅 Debug 主动启用 D3D12 调试层，不向通用 Renderer 传播后端配置。
target_compile_definitions(MEngineD3D12RHI
    PRIVATE
        MENGINE_D3D12_DEBUG_LAYER=$<CONFIG:Debug>
)

if(MSVC)
    # 源文件包含教学用中文注释；明确按 UTF-8 解析，避免系统代码页误读注释。
    target_compile_options(MEngineD3D12RHI
        PRIVATE
            /utf-8
    )
endif()

target_include_directories(MEngineD3D12RHI
    PUBLIC
        ${PROJECT_SOURCE_DIR}/Engine
)

target_link_libraries(MEngineD3D12RHI
    PUBLIC
        MEngineRHI
    PRIVATE
        d3d12
        dxgi
)
