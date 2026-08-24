add_executable(MEngineGame
    GameMain.cpp
)

target_link_libraries(MEngineGame
    PRIVATE
        MEngineLaunch
        MEngineRuntime
)
