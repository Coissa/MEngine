add_library(MEngineRenderCore INTERFACE)

target_sources(MEngineRenderCore
    INTERFACE
        GizmoRenderData.h
        RenderExtent.h
        RenderFrame.h
        RenderHandles.h
        RenderItem.h
        RenderView.h
        UIRenderData.h
)

target_compile_features(MEngineRenderCore
    INTERFACE
        cxx_std_20
)

if(MSVC)
    # RenderFrame 等公共头包含中文教学注释，使用者必须按 UTF-8 解析。
    target_compile_options(MEngineRenderCore INTERFACE /utf-8)
endif()

target_include_directories(MEngineRenderCore
    INTERFACE
        ${PROJECT_SOURCE_DIR}/Engine
)

target_link_libraries(MEngineRenderCore
    INTERFACE
        MEngineCore
)
