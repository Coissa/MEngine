add_library(MEngineRuntime STATIC
    Engine.cpp
    Engine.h
    GameInstance.cpp
    GameInstance.h
    GameEngine.cpp
    GameEngine.h
    World.cpp
    World.h
    WorldContext.cpp
    WorldContext.h
    WorldType.h
)

target_include_directories(MEngineRuntime
    PUBLIC
        ${PROJECT_SOURCE_DIR}/Engine
)
