add_library(MEngineSerialization STATIC
    BuiltinSerialization.cpp
    SerializationRegistry.cpp
    Serializer.cpp
)

target_include_directories(MEngineSerialization
    PUBLIC
        ${PROJECT_SOURCE_DIR}/Engine
)

target_link_libraries(MEngineSerialization
    PUBLIC
        MEngineCore
        MEngineReflection
)

if(MSVC)
    target_compile_options(MEngineSerialization PRIVATE /EHsc /utf-8)
endif()
