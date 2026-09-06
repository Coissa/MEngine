add_library(MEngineEditorRuntime STATIC
    EditorCamera.cpp
    EditorCamera.h
    EditorCommand.h
    EditorEngine.cpp
    EditorEngine.h
    EditorScenePicker.cpp
    EditorScenePicker.h
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
    D3D12EditorUIResources.cpp
    D3D12EditorUIResources.h
    EditorMain.cpp
)

if(MSVC)
    target_compile_options(MEngineEditor
        PRIVATE
            /EHsc
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
