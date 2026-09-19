add_library(MEngineRuntime STATIC
    BuiltinScene.cpp
    BuiltinScene.h
    Component.cpp
    Component.h
    ComponentRegistry.cpp
    ComponentRegistry.h
    Engine.cpp
    Engine.h
    EngineServices.h
    FixedStepScheduler.cpp
    FixedStepScheduler.h
    Generated/Reflection/RuntimeReflection.generated.cpp
    Generated/Reflection/RuntimeReflection.generated.h
    GameObject.cpp
    GameObject.h
    GameObjectHandle.h
    SceneSerializer.cpp
    SceneSerializer.h
    SceneMath.cpp
    SceneMath.h
    GameInstance.cpp
    GameInstance.h
    GameEngine.cpp
    GameEngine.h
    World.cpp
    World.h
    WorldContext.cpp
    WorldContext.h
    WorldType.h
    WorldRenderExtractor.cpp
    WorldRenderExtractor.h
)

target_include_directories(MEngineRuntime
    PUBLIC
        ${PROJECT_SOURCE_DIR}/Engine
)

target_link_libraries(MEngineRuntime
    PUBLIC
        MEngineAsset
        MEngineCore
        MEngineReflection
        MEngineSerialization
        MEngineRenderCore
)

if(MSVC)
    target_compile_options(MEngineRuntime PRIVATE /EHsc /utf-8)
endif()
