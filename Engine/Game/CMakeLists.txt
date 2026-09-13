add_executable(MEngineGame
    GameMain.cpp
)

mengine_package_shaders(MEngineGame)
mengine_package_assets(MEngineGame)

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
