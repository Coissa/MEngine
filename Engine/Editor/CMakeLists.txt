add_library(MEngineEditorRuntime STATIC
    EditorCamera.cpp
    EditorCamera.h
    EditorCommand.h
    EditorEngine.cpp
    EditorEngine.h
    EditorGizmo.cpp
    EditorGizmo.h
    EditorScenePicker.cpp
    EditorScenePicker.h
    EditorTransaction.cpp
    EditorTransaction.h
    EditorUserInterface.cpp
    EditorUserInterface.h
)

target_include_directories(MEngineEditorRuntime
    PUBLIC
        ${PROJECT_SOURCE_DIR}/Engine
)

target_link_libraries(MEngineEditorRuntime
    PUBLIC
        MEngineRuntime
    PRIVATE
        MEngineImGui
        MEnginePlatformWindows
)

add_executable(MEngineEditor
    D3D12EditorRenderResources.cpp
    D3D12EditorRenderResources.h
    EditorMain.cpp
)

mengine_package_shaders(MEngineEditor)
mengine_package_assets(MEngineEditor)

if(MSVC)
    target_compile_options(MEngineEditorRuntime
        PRIVATE
            /EHsc
            /utf-8
    )
    target_compile_options(MEngineEditor
        PRIVATE
            /EHsc
            /utf-8
    )
endif()

target_link_libraries(MEngineEditor
    PRIVATE
        MEngineD3D12RHI
        MEngineD3D12RendererExample
        MEngineLaunch
        MEngineEditorRuntime
        MEnginePlatformWindows
        MEngineRenderer
        d3dcompiler
)
