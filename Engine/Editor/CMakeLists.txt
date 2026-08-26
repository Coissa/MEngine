add_library(MEngineEditorRuntime STATIC
    EditorEngine.cpp
    EditorEngine.h
)

target_include_directories(MEngineEditorRuntime
    PUBLIC
        ${PROJECT_SOURCE_DIR}/Engine
)

target_link_libraries(MEngineEditorRuntime
    PUBLIC
        MEngineRuntime
)

add_executable(MEngineEditor
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
        MEngineLaunch
        MEngineEditorRuntime
)
