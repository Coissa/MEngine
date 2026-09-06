add_executable(MEngineGame
    GameMain.cpp
)

if(MSVC)
    target_compile_options(MEngineGame
        PRIVATE
            /EHsc
    )
endif()

target_link_libraries(MEngineGame
    PRIVATE
        MEngineD3D12RHI
        MEngineD3D12RendererExample
        MEngineLaunch
        MEnginePlatformWindows
        MEngineRenderer
        MEngineRuntime
)
